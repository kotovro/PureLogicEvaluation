#include "app_main.h"

#include <cstdint>

#include "main.h"

namespace {
std::uint32_t g_lastToggle = 0;
}  // namespace

extern "C" void app_init(void)
{
    HAL_DBGMCU_EnableDBGSleepMode();
    g_lastToggle = HAL_GetTick();
}

extern "C" void app_step(void)
{
    if (HAL_GetTick() - g_lastToggle >= 500u) {
        g_lastToggle += 500u;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
    __WFI();
}
