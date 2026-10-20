// п.4Б, 4В: среднее по 10 измерениям и пересчёт в °C и мВ.
#ifndef LOGIC_SENSORS_HPP
#define LOGIC_SENSORS_HPP

#include <cstdint>

#include "logic/config.hpp"

namespace logic {

// Суммы кодов по последнему полному блоку из 10 измерений.
// Хранятся суммы, а не средние: делим один раз в конце, в convert().
struct AdcSums {
    std::uint32_t vref  = 0;
    std::uint32_t ts    = 0;
    bool          ready = false;  // false — первый блок ещё не набран
};

// Блочное среднее: копим 10 измерений, запоминаем суммы, начинаем заново.
// push() вызывается из прерывания АЦП.
class AdcAverager {
public:
    void push(std::uint16_t vref, std::uint16_t ts)
    {
        sumVref_ += vref;
        sumTs_ += ts;
        if (++count_ == cfg::kAdcWindow) {
            last_.vref  = sumVref_;
            last_.ts    = sumTs_;
            last_.ready = true;
            sumVref_    = 0;
            sumTs_      = 0;
            count_      = 0;
        }
    }

    AdcSums result() const { return last_; }

private:
    std::uint32_t sumVref_ = 0;
    std::uint32_t sumTs_   = 0;
    unsigned      count_   = 0;  // измерений в текущем блоке
    AdcSums       last_;
};

// Заводские калибровки (системная память, сняты при VDDA = 3.3 В).
struct SensorCal {
    std::uint16_t vrefint;  // код VREFINT
    std::uint16_t ts30;     // код датчика при 30 °C
    std::uint16_t ts110;    // код датчика при 110 °C
};

struct Reading {
    bool         valid;  // false — ещё нет полного блока или калибровки пустые
    std::int32_t tempC;
    std::int32_t vddaMv;
};

Reading convert(const AdcSums& sums, const SensorCal& cal);

}  // namespace logic

#endif
