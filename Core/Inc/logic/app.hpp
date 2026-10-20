// Вся логика ТЗ. Не знает про HAL: железо — через AppIo, время — параметром.
// Поэтому тот же код работает в прошивке (app_main.cpp) и в тестах на ПК.
// Все методы вызываются только из основного цикла.
#ifndef LOGIC_APP_HPP
#define LOGIC_APP_HPP

#include <cstdint>

#include "logic/led_timer.hpp"

namespace logic {

// Связь логики с железом. В прошивке — BoardIo, в тестах — FakeIo.
class AppIo {
public:
    virtual void setLed(bool on) = 0;

protected:
    ~AppIo() = default;  // удалять через AppIo* некому: кучи нет
};

class App {
public:
    explicit App(AppIo& io);

    void start(std::uint32_t now);  // светодиод включён (п.1А)
    void tick(std::uint32_t now);   // вызывать каждую миллисекунду

private:
    void toggleLed(std::uint32_t now);

    AppIo&   io_;
    LedTimer led_;
};

}  // namespace logic

#endif
