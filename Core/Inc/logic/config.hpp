// Константы из ТЗ в одном месте.
#ifndef LOGIC_CONFIG_HPP
#define LOGIC_CONFIG_HPP

#include <cstdint>

namespace cfg {

constexpr std::uint32_t kLedOnMs  = 4000;  // п.1: время свечения по умолчанию
constexpr std::uint32_t kLedOffMs = 6000;  // п.1: время тёмного состояния по умолчанию

}  // namespace cfg

#endif
