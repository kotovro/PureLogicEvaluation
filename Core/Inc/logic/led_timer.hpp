// Таймер состояний светодиода (п.1, 2Г, 5Б/В).
// Длительность состояния фиксируется при входе в него, поэтому новые
// значения из команд применяются только со следующей смены состояния.
#ifndef LOGIC_LED_TIMER_HPP
#define LOGIC_LED_TIMER_HPP

#include <cstdint>

namespace logic {

class LedTimer {
public:
    LedTimer(std::uint32_t onMs, std::uint32_t offMs) : onMs_(onMs), offMs_(offMs) {}

    void start(std::uint32_t now) { enter(true, now); }

    bool expired(std::uint32_t now) const { return now - since_ >= duration_; }

    // Переключить; возвращает, сколько длилось завершившееся состояние.
    std::uint32_t toggle(std::uint32_t now)
    {
        const std::uint32_t elapsed = now - since_;
        enter(!on_, now);
        return elapsed;
    }

    bool          isOn() const { return on_; }
    std::uint32_t onMs() const { return onMs_; }
    std::uint32_t offMs() const { return offMs_; }
    void          setOnMs(std::uint32_t ms) { onMs_ = ms; }
    void          setOffMs(std::uint32_t ms) { offMs_ = ms; }

private:
    void enter(bool on, std::uint32_t now)
    {
        on_       = on;
        since_    = now;
        duration_ = on ? onMs_ : offMs_;
    }

    std::uint32_t onMs_;
    std::uint32_t offMs_;
    bool on_  = true;
    std::uint32_t since_    = 0;
    std::uint32_t duration_ = 0;
};

}  // namespace logic

#endif
