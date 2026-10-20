#include "logic/app.hpp"

#include "logic/config.hpp"

namespace logic {

App::App(AppIo& io) : io_(io), led_(cfg::kLedOnMs, cfg::kLedOffMs)
{
}

void App::toggleLed(std::uint32_t now)
{
    led_.toggle(now);
    io_.setLed(led_.isOn());
}

void App::start(std::uint32_t now)
{
    led_.start(now);
    io_.setLed(true);
}

void App::tick(std::uint32_t now)
{
    // п.1: смена состояния по таймеру.
    if (led_.expired(now)) {
        toggleLed(now);
    }
}

}  // namespace logic
