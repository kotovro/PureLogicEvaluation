#include "logic/command_parser.hpp"

#include "logic/config.hpp"

namespace logic {

Command CommandParser::error()
{
    len_ = 0;
    Command c;
    c.type = Command::Type::Error;
    return c;
}

Command CommandParser::feed(std::uint8_t ch, bool rxError)
{
    if (rxError) {
        return error();
    }

    if (ch == '\r' || ch == '\n') {
        if (len_ > 0) {
            return error();  // число оборвано концом строки
        }
        return Command{};
    }

    if (ch == '?' && len_ == 0) {
        Command c;
        c.type = Command::Type::Query;
        return c;
    }

    if (ch >= '0' && ch <= '9') {
        if (len_ == sizeof(digits_)) {
            return error();  // больше 4 цифр
        }
        digits_[len_++] = static_cast<char>(ch);
        return Command{};
    }

    if ((ch == 'L' || ch == 'D') && len_ > 0) {
        std::uint32_t value = 0;
        for (std::uint8_t i = 0; i < len_; i++) {
            value = value * 10u + static_cast<std::uint32_t>(digits_[i] - '0');
        }
        if (value < cfg::kLedMsMin || value > cfg::kLedMsMax) {
            return error();
        }

        Command c;
        c.type  = (ch == 'L') ? Command::Type::SetOn : Command::Type::SetOff;
        c.value = value;
        for (std::uint8_t i = 0; i < len_; i++) {
            c.text[i] = digits_[i];
        }
        c.text[len_] = static_cast<char>(ch);
        len_         = 0;
        return c;
    }

    return error();
}

}  // namespace logic
