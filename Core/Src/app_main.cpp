// Связь logic::App с железом через HAL. Единственный файл, который знает
// и про HAL, и про логику.
//
// Модель: суперцикл + прерывания. Прерывания только складывают данные,
// вся логика — в основном цикле.
#include "app_main.h"

#include <cstring>

#include "logic/app.hpp"
#include "main.h"
#include "usart.h"

namespace {

// ---- реализация AppIo поверх HAL ----------------------------------------------

class BoardIo : public logic::AppIo {
public:
    void setLed(bool on) override
    {
        // PC13: светодиод горит при НИЗКОМ уровне.
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    void print(const char* text) override
    {
        // Блокирующий вывод: строка ~20 символов на 115200 8E1 — ~2 мс.
        // Пропущенные за это время миллисекунды основной цикл догоняет.
        HAL_UART_Transmit(&huart1, reinterpret_cast<const std::uint8_t*>(text),
                          static_cast<std::uint16_t>(std::strlen(text)), 100);
    }
};

BoardIo       g_io;
logic::App    g_app(g_io);
std::uint32_t g_lastTick = 0;

}  // namespace

// ---- вызовы из main.c -----------------------------------------------------------

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
        const bool pressed = HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET;  // кнопка на GND
        g_app.tick(g_lastTick, pressed);
    }

    __WFI();  // спим до прерывания; SysTick будит каждую 1 мс
}
