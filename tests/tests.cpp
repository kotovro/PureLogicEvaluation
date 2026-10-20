// Тесты логики на ПК, без платы:
//   g++ -std=c++14 -Wall -Wextra -I../Core/Inc ../Core/Src/logic/*.cpp tests.cpp -o tests && ./tests
//
// App получает время параметром, а железо — через AppIo, поэтому здесь
// время моделируется циклом по миллисекундам, а вывод собирается в строку.
#include <cstdio>
#include <string>

#include "logic/app.hpp"

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
            app.tick(now);
        }
    }

    FakeIo        io;
    App           app;
    std::uint32_t now = 0;
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

int main()
{
    test_blink();
    std::printf(g_failed == 0 ? "All tests passed\n" : "%d check(s) failed\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
