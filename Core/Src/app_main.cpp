// Связь logic::App с железом через HAL. Единственный файл, который знает
// и про HAL, и про логику.
//
// Модель: цикл + прерывания. Прерывания только складывают данные,
// вся логика — в основном цикле.
#include "app_main.h"

#include "logic/app.hpp"
#include "main.h"

namespace {

// реализация AppIo поверх HAL

class BoardIo : public logic::AppIo {
public:
    void setLed(bool on) override
    {
        // PC13: светодиод горит при НИЗКОМ уровне.
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
};

BoardIo       g_io;
logic::App    g_app(g_io);
std::uint32_t g_lastTick = 0;

}  // namespace

// вызовы из main.c

extern "C" void app_init(void)
{
    // Основной цикл спит в WFI; без этого отладчик теряет связь с ядром во сне.
    HAL_DBGMCU_EnableDBGSleepMode();

    g_lastTick = HAL_GetTick();
    g_app.start(g_lastTick);
}

extern "C" void app_step(void)
{
    // Логика рассчитана на шаг 1 мс. Если итерация задержалась, вызываем
    // tick() за каждую пропущенную миллисекунду.
    const std::uint32_t now = HAL_GetTick();
    while (g_lastTick != now) {
        g_lastTick++;
        g_app.tick(g_lastTick);
    }

    __WFI();  // спим до прерывания; SysTick будит каждую 1 мс
}
