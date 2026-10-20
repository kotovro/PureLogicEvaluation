// Подавление дребезга: новое состояние кнопки принимается, если сырой
// уровень непрерывно держался debounceMs. Любой отскок перезапускает отсчёт.
#ifndef LOGIC_DEBOUNCER_HPP
#define LOGIC_DEBOUNCER_HPP

#include <cstdint>

namespace logic {

class Debouncer {
public:
    enum class Event { None, Pressed, Released };

    explicit Debouncer(std::uint32_t debounceMs) : debounceMs_(debounceMs) {}

    Event update(bool rawPressed, std::uint32_t now)
    {
        if (rawPressed == stable_) {
            changing_ = false;  // вернулись к принятому уровню: это был дребезг
            return Event::None;
        }
        if (!changing_) {
            changing_ = true;  // уровень только что изменился: начинаем отсчёт
            since_    = now;
            return Event::None;
        }
        // Беззнаковая разность корректна и при переполнении счётчика мс.
        if (now - since_ < debounceMs_) {
            return Event::None;
        }
        stable_   = rawPressed;
        changing_ = false;
        return stable_ ? Event::Pressed : Event::Released;
    }

    bool pressed() const { return stable_; }

private:
    std::uint32_t debounceMs_;
    std::uint32_t since_    = 0;
    bool          stable_   = false;  // принятое состояние, true = нажата
    bool          changing_ = false;  // сырой уровень отличается от stable_
};

}  // namespace logic

#endif
