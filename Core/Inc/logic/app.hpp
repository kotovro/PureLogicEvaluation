// Вся логика ТЗ. Не знает про HAL: железо — через AppIo, время — параметром.
// Поэтому тот же код работает в прошивке (app_main.cpp) и в тестах на ПК.
// Все методы вызываются только из основного цикла.
#ifndef LOGIC_APP_HPP
#define LOGIC_APP_HPP

#include <cstdint>

#include "logic/command_parser.hpp"
#include "logic/debouncer.hpp"
#include "logic/led_timer.hpp"
#include "logic/sensors.hpp"

namespace logic {

// Связь логики с железом. В прошивке — BoardIo, в тестах — FakeIo.
class AppIo {
public:
    virtual void    setLed(bool on)         = 0;
    virtual void    print(const char* text) = 0;  // текст уходит в UART как есть
    virtual Reading readSensors()           = 0;

protected:
    ~AppIo() = default;  // удалять через AppIo* некому: кучи нет
};

class App {
public:
    explicit App(AppIo& io);

    void start(std::uint32_t now);                     // LED ON + "LED ON" (п.1А)
    void tick(std::uint32_t now, bool buttonPressed);  // вызывать каждую миллисекунду
    void onByte(std::uint8_t ch, bool rxError);        // принятый по UART байт

private:
    // printf-форматирование + "\r\n". Атрибут — чтобы компилятор проверял
    // соответствие %lu/%ld аргументам (на ARM uint32_t — это unsigned long).
    void say(const char* format, ...) __attribute__((format(printf, 2, 3)));
    void toggleLed(std::uint32_t now);
    void reportSensors(bool tempFirst);

    AppIo&        io_;
    Debouncer     button_;
    LedTimer      led_;
    CommandParser parser_;

    std::uint32_t clicks_          = 0;      // п.2В: номер клика, считаем по нажатиям
    bool          toggleOnRelease_ = false;  // текущий клик переключает на отпускании
    std::uint32_t pressedAt_       = 0;
    bool          longReported_    = false;  // TEMP/VCC за это нажатие уже выведены
};

}  // namespace logic

#endif
