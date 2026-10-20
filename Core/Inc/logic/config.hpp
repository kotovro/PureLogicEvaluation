// Константы из ТЗ в одном месте.
#ifndef LOGIC_CONFIG_HPP
#define LOGIC_CONFIG_HPP

#include <cstdint>

namespace cfg {

constexpr std::uint32_t kLedOnMs  = 4000;  // п.1: время свечения по умолчанию
constexpr std::uint32_t kLedOffMs = 6000;  // п.1: время тёмного состояния по умолчанию
constexpr std::uint32_t kLedMsMin = 500;   // п.5Г: допустимый диапазон
constexpr std::uint32_t kLedMsMax = 9999;

constexpr std::uint32_t kDebounceMs  = 10;   // п.2Б: реакция 10-20 мс при требовании <= 50
constexpr std::uint32_t kLongPressMs = 500;  // п.4Г: "превысит 500 ms" -> строго больше

constexpr unsigned kAdcWindow = 10;  // п.4Б: среднее по 10 измерениям (блоками)

}  // namespace cfg

#endif
