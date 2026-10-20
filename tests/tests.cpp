// Тесты логики на ПК, без платы:
//   g++ -std=c++14 -Wall -Wextra -I../Core/Inc ../Core/Src/logic/*.cpp tests.cpp -o tests && ./tests
//
// App получает время параметром, а железо — через AppIo, поэтому здесь
// время моделируется циклом по миллисекундам, а вывод собирается в строку.
#include <cstdio>
#include <string>

#include "logic/app.hpp"
#include "logic/command_parser.hpp"
#include "logic/sensors.hpp"

using namespace logic;

static int g_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            g_failed++;                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

// Подмена железа.
class FakeIo : public AppIo {
public:
    void    setLed(bool on) override { led = on; }
    void    print(const char* text) override { out += text; }
    Reading readSensors() override { return Reading{true, 74, 3402}; }  // числа из примера п.4Г

    bool        led = false;
    std::string out;
};

// Модель платы: App::tick каждую миллисекунду, как в app_step().
struct Board {
    Board() : app(io) { app.start(0); }

    void runTo(std::uint32_t t)
    {
        while (now < t) {
            now++;
            app.tick(now, button);
        }
    }
    void send(const char* s)
    {
        for (; *s != '\0'; ++s) {
            app.onByte(static_cast<std::uint8_t>(*s), false);
        }
    }

    FakeIo        io;
    App           app;
    std::uint32_t now    = 0;
    bool          button = false;
};

static void test_blink()
{
    Board b;
    CHECK(b.io.led);
    b.runTo(3999);
    CHECK(b.io.led);
    b.runTo(4000);
    CHECK(!b.io.led);  // 4 с горит
    b.runTo(9999);
    CHECK(!b.io.led);
    b.runTo(10000);
    CHECK(b.io.led);  // 6 с не горит
    CHECK(b.io.out ==
          "LED ON\r\n"
          "LED ON TIME 4000\r\nLED OFF\r\n"
          "LED OFF TIME 6000\r\nLED ON\r\n");
}

// Дословно пример из п.3 ТЗ. Нажатие на 1254-й мс свечения: сырой фронт
// в 11244 + 10 мс фильтра; удержание 73 мс.
static void test_spec_example()
{
    Board b;
    b.runTo(11243);
    b.button = true;
    b.runTo(11316);
    b.button = false;
    b.runTo(17254);
    CHECK(b.io.out ==
          "LED ON\r\n"
          "LED ON TIME 4000\r\n"
          "LED OFF\r\n"
          "LED OFF TIME 6000\r\n"
          "LED ON\r\n"
          "BUTTON ON\r\n"
          "LED ON TIME 1254\r\n"
          "LED OFF\r\n"
          "BUTTON OFF\r\n"
          "BUTTON TIME 73\r\n"
          "LED OFF TIME 6000\r\n"
          "LED ON\r\n");
}

// п.2В: клики 1, 2 переключают на нажатии, 3, 4 — на отпускании; п.2Б: <= 50 мс.
static void test_click_sequence()
{
    Board       b;
    const char* expected = "PPRRPPRR";
    for (std::uint32_t i = 0; i < 8; i++) {
        const std::uint32_t down = 1000 + i * 300;
        b.runTo(down - 1);
        const bool before = b.io.led;
        b.button = true;
        b.runTo(down + 50);  // 50 мс после нажатия
        const bool onPress = b.io.led != before;
        b.button = false;
        b.runTo(down + 200);
        const bool onRelease = !onPress && b.io.led != before;
        CHECK((expected[i] == 'P') ? onPress : onRelease);
    }
}

// Подать строку в парсер, вернуть последнюю непустую команду.
static Command parse(const char* s)
{
    CommandParser p;
    Command       last;
    for (; *s != '\0'; ++s) {
        const Command c = p.feed(static_cast<std::uint8_t>(*s), false);
        if (c.type != Command::Type::None) {
            last = c;
        }
    }
    return last;
}

static void test_parser()
{
    Command c = parse("3500L");
    CHECK(c.type == Command::Type::SetOn && c.value == 3500 && std::string(c.text) == "3500L");

    c = parse("0600D");  // ведущий ноль допустим, в ответе строка как принята
    CHECK(c.type == Command::Type::SetOff && c.value == 600 && std::string(c.text) == "0600D");

    CHECK(parse("?").type == Command::Type::Query);
    CHECK(parse("500L").type == Command::Type::SetOn);   // граница
    CHECK(parse("9999D").type == Command::Type::SetOff);  // граница
    CHECK(parse("\r\n").type == Command::Type::None);   // пустые строки игнорируются

    const char* bad[] = {"499L", "10000", "L", "3500l", "35\r", "x"};
    for (const char* s : bad) {
        CHECK(parse(s).type == Command::Type::Error);
    }

    CommandParser p;  // байт с ошибкой чётности
    CHECK(p.feed('?', true).type == Command::Type::Error);
}

// п.4Г: TEMP/VCC один раз, когда удержание превысило 500 мс.
static void test_long_press()
{
    Board b;
    b.runTo(999);
    b.button = true;  // фильтр примет нажатие в 1010
    b.runTo(1510);
    CHECK(b.io.out.find("TEMP") == std::string::npos);  // ровно 500 — ещё нет
    b.io.out.clear();
    b.runTo(1511);
    CHECK(b.io.out == "TEMP 74\r\nVCC 3402\r\n");
    b.io.out.clear();
    b.runTo(3000);
    CHECK(b.io.out.find("TEMP") == std::string::npos);  // повторно не выводится
}

// п.5: ответы на команды и применение после смены состояния.
static void test_commands()
{
    Board b;
    b.runTo(100);
    b.io.out.clear();
    b.send("?");
    CHECK(b.io.out == "VCC 3402\r\nTEMP 74\r\nLED 4000L\r\nLED 6000D\r\n");

    b.io.out.clear();
    b.send("3500L\r\n5500D");
    CHECK(b.io.out == "LED 3500L OK\r\nLED 5500D OK\r\n");

    b.io.out.clear();
    b.runTo(4000 + 5500 + 3500);  // текущий ON доигрывает 4000, дальше новые времена
    CHECK(b.io.out ==
          "LED ON TIME 4000\r\nLED OFF\r\n"
          "LED OFF TIME 5500\r\nLED ON\r\n"
          "LED ON TIME 3500\r\nLED OFF\r\n");

    b.io.out.clear();  // ошибка: ERROR, заданные времена не меняются
    b.send("499L?");
    CHECK(b.io.out == "ERROR\r\nVCC 3402\r\nTEMP 74\r\nLED 3500L\r\nLED 5500D\r\n");

    b.io.out.clear();  // байт с ошибкой чётности
    b.app.onByte('?', true);
    CHECK(b.io.out == "ERROR\r\n");
}

static void test_sensor_math()
{
    const SensorCal cal{1500, 950, 1200};
    AdcAverager     avg;

    for (int i = 0; i < 9; i++) avg.push(1500, 950);
    CHECK(!convert(avg.result(), cal).valid);  // 9 измерений — блок ещё не набран

    avg.push(1500, 950);  // 10-е: точка калибровки 30 °C, 3.3 В
    Reading r = convert(avg.result(), cal);
    CHECK(r.valid && r.vddaMv == 3300 && r.tempC == 30);

    // VDDA = 3.0 В: VREFINT = 1500*3300/3000 = 1650; датчик при 70 °C даёт код
    // 1075 на 3.3 В, т.е. 1182.5 на 3.0 В. Без компенсации VDDA было бы ~100 °C.
    for (int i = 0; i < 10; i++) avg.push(1650, (i % 2) ? 1183 : 1182);
    r = convert(avg.result(), cal);
    CHECK(r.valid && r.vddaMv == 3000 && r.tempC == 70);

    // Неполный следующий блок результат не меняет.
    for (int i = 0; i < 5; i++) avg.push(1000, 100);
    r = convert(avg.result(), cal);
    CHECK(r.valid && r.vddaMv == 3000 && r.tempC == 70);

    CHECK(!convert(avg.result(), SensorCal{0xFFFF, 0xFFFF, 0xFFFF}).valid);  // пустые калибровки
}

int main()
{
    test_blink();
    test_spec_example();
    test_click_sequence();
    test_long_press();
    test_parser();
    test_commands();
    test_sensor_math();
    std::printf(g_failed == 0 ? "All tests passed\n" : "%d check(s) failed\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
