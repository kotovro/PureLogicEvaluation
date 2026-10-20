// Тесты логики на ПК, без платы:
//   g++ -std=c++14 -Wall -Wextra -I../Core/Inc ../Core/Src/logic/*.cpp tests.cpp -o tests && ./tests
//
// App получает время параметром, а железо — через AppIo, поэтому здесь
// время моделируется циклом по миллисекундам, а вывод собирается в строку.
#include <cstdio>
#include <string>

#include "logic/app.hpp"
#include "logic/command_parser.hpp"

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
    void setLed(bool on) override { led = on; }
    void print(const char* text) override { out += text; }

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

int main()
{
    test_blink();
    test_spec_example();
    test_click_sequence();
    test_parser();
    std::printf(g_failed == 0 ? "All tests passed\n" : "%d check(s) failed\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
