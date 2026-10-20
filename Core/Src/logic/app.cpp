#include "logic/app.hpp"

#include <cstdarg>
#include <cstdio>

#include "logic/config.hpp"

namespace logic {

App::App(AppIo& io)
    : io_(io), button_(cfg::kDebounceMs), led_(cfg::kLedOnMs, cfg::kLedOffMs)
{
}

void App::say(const char* format, ...)
{
    char    line[32];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);  // не выйдет за буфер, в худшем случае обрежет
    va_end(args);
    io_.print(line);
    io_.print("\r\n");
}

void App::toggleLed(std::uint32_t now)
{
    const bool          wasOn   = led_.isOn();
    const unsigned long elapsed = led_.toggle(now);

    io_.setLed(led_.isOn());  // сначала светодиод — от этого момента считается реакция
    say("LED %s TIME %lu", wasOn ? "ON" : "OFF", elapsed);  // п.3
    say("LED %s", led_.isOn() ? "ON" : "OFF");
}

void App::start(std::uint32_t now)
{
    led_.start(now);
    io_.setLed(true);
    say("LED ON");
}

void App::tick(std::uint32_t now, bool buttonPressed)
{
    switch (button_.update(buttonPressed, now)) {
    case Debouncer::Event::Pressed: {
        pressedAt_ = now;
        say("BUTTON ON");
        // п.2В: клики 1 и 2 переключают на нажатии, 3 и 4 — на отпускании, и снова.
        const bool onPress = (clicks_ % 4) < 2;
        clicks_++;
        toggleOnRelease_ = !onPress;
        if (onPress) {
            toggleLed(now);
        }
        break;
    }
    case Debouncer::Event::Released:
        say("BUTTON OFF");
        // Оба фронта задержаны фильтром на одинаковые 10 мс — разность точная.
        say("BUTTON TIME %lu", static_cast<unsigned long>(now - pressedAt_));
        if (toggleOnRelease_) {
            toggleOnRelease_ = false;
            toggleLed(now);
        }
        break;
    case Debouncer::Event::None:
        break;
    }

    // п.1: смена состояния по таймеру.
    if (led_.expired(now)) {
        toggleLed(now);
    }
}

void App::onByte(std::uint8_t ch, bool rxError)
{
    const Command cmd = parser_.feed(ch, rxError);
    switch (cmd.type) {
    case Command::Type::Query:  // п.5А
        say("LED %luL", static_cast<unsigned long>(led_.onMs()));
        say("LED %luD", static_cast<unsigned long>(led_.offMs()));
        break;
    case Command::Type::SetOn:  // п.5Б: применится со следующей смены состояния
        led_.setOnMs(cmd.value);
        say("LED %s OK", cmd.text);
        break;
    case Command::Type::SetOff:  // п.5В
        led_.setOffMs(cmd.value);
        say("LED %s OK", cmd.text);
        break;
    case Command::Type::Error:  // п.5Д
        say("ERROR");
        break;
    case Command::Type::None:
        break;
    }
}

}  // namespace logic
