// п.5: побайтовый разбор команд из UART.
//   ?            -> запрос состояния (сразу, без Enter)
//   1-4 цифры + L -> задать время свечения
//   1-4 цифры + D -> задать время тёмного состояния
//   CR, LF       -> вне команды игнорируются
// Любой другой символ, байт с ошибкой приёма, 5-я цифра, L/D без цифр,
// значение вне 500..9999 -> Error.
#ifndef LOGIC_COMMAND_PARSER_HPP
#define LOGIC_COMMAND_PARSER_HPP

#include <cstdint>

namespace logic {

struct Command {
    enum class Type { None, Query, SetOn, SetOff, Error };

    Type          type  = Type::None;
    std::uint32_t value = 0;
    char          text[6] = {};  // принятая строка как есть, например "3500L"
};

class CommandParser {
public:
    // rxError — байт пришёл с ошибкой чётности/кадра/переполнения.
    Command feed(std::uint8_t ch, bool rxError);

private:
    Command error();

    char         digits_[4] = {};
    std::uint8_t len_       = 0;
};

}  // namespace logic

#endif
