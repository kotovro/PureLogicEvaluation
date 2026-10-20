#include "logic/sensors.hpp"

namespace logic {

namespace {

// Деление с округлением к ближайшему; b > 0.
std::int64_t divRound(std::int64_t a, std::int64_t b)
{
    return (a >= 0) ? (a + b / 2) / b : -((-a + b / 2) / b);
}

}  // namespace

// Формулы (RM0383, DS10314):
//   VDDA = 3300 * VREFINT_CAL / VREFINT
//   TS33 = TS * VREFINT_CAL / VREFINT       — код датчика, приведённый к VDDA = 3.3 В
//   T    = 30 + (TS33 - TS_CAL1) * 80 / (TS_CAL2 - TS_CAL1)
// Вместо средних подставляются суммы: число измерений сокращается, а деление
// одно — в конце, с округлением. Произведения доходят до ~5e9, поэтому int64.
Reading convert(const AdcSums& s, const SensorCal& cal)
{
    // Пустая системная память читается как 0xFFFF.
    const bool calOk = cal.vrefint > 0 && cal.vrefint < 4095 && cal.ts110 > cal.ts30 && cal.ts110 < 4095;
    if (!s.ready || s.vref == 0 || !calOk) {
        return Reading{false, 0, 0};
    }

    const std::int64_t n       = cfg::kAdcWindow;
    const std::int64_t sumVref = s.vref;
    const std::int64_t sumTs   = s.ts;
    const std::int64_t vcal    = cal.vrefint;
    const std::int64_t c1      = cal.ts30;
    const std::int64_t c2      = cal.ts110;

    const std::int64_t vdda = divRound(3300 * vcal * n, sumVref);
    const std::int64_t temp = 30 + divRound(80 * (sumTs * vcal - c1 * sumVref), (c2 - c1) * sumVref);

    return Reading{true, static_cast<std::int32_t>(temp), static_cast<std::int32_t>(vdda)};
}

}  // namespace logic
