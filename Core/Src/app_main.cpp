// Связь logic::App с железом через HAL. Единственный файл, который знает
// и про HAL, и про логику.
//
// Модель: суперцикл + прерывания. Прерывания только складывают данные
// (байты UART, измерения АЦП), вся логика — в основном цикле.
#include "app_main.h"

#include <atomic>
#include <cstring>

#include "adc.h"
#include "logic/app.hpp"
#include "main.h"
#include "tim.h"
#include "usart.h"

namespace {

// ---- UART RX: кольцо, пишет прерывание, читает основной цикл -----------------

struct RxItem {
    std::uint8_t ch;
    bool         error;
};

constexpr std::uint32_t    kRxSize = 64;
RxItem                     g_rxBuf[kRxSize];
std::atomic<std::uint32_t> g_rxHead{0};  // пишет только прерывание
std::atomic<std::uint32_t> g_rxTail{0};  // пишет только основной цикл
std::uint8_t               g_rxByte;     // буфер на 1 байт для HAL_UART_Receive_IT

void rxStart()
{
    HAL_UART_Receive_IT(&huart1, &g_rxByte, 1);
}

void rxPush(std::uint8_t ch, bool error)
{
    const std::uint32_t head = g_rxHead.load();
    if (head - g_rxTail.load() < kRxSize) {  // если кольцо полно — байт теряется
        g_rxBuf[head % kRxSize] = RxItem{ch, error};
        g_rxHead.store(head + 1);  // atomic: индекс станет виден после записи данных
    }
}

bool rxPop(RxItem& out)
{
    const std::uint32_t tail = g_rxTail.load();
    if (tail == g_rxHead.load()) {
        return false;
    }
    out = g_rxBuf[tail % kRxSize];
    g_rxTail.store(tail + 1);
    return true;
}

// ---- АЦП: TIM2 75 Гц -> ADC1 injected [VREFINT, датчик] -> прерывание -------

logic::AdcAverager g_adc;  // пишет прерывание, читает основной цикл с запретом прерываний
logic::SensorCal  g_cal;

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
        // Приём при этом идёт по прерываниям, а пропущенные миллисекунды
        // основной цикл догоняет, так что таймингам это не мешает.
        HAL_UART_Transmit(&huart1, reinterpret_cast<const std::uint8_t*>(text),
                          static_cast<std::uint16_t>(std::strlen(text)), 100);
    }

    logic::Reading readSensors() override
    {
        // Две суммы должны быть из одного блока. Без запрета прерываний АЦП
        // может закончить блок между чтением первой и второй суммы.
        __disable_irq();
        const logic::AdcSums sums = g_adc.result();
        __enable_irq();
        return logic::convert(sums, g_cal);
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

    // Заводские калибровки (адреса — макросы из stm32f4xx_ll_adc.h).
    g_cal = logic::SensorCal{*VREFINT_CAL_ADDR, *TEMPSENSOR_CAL1_ADDR, *TEMPSENSOR_CAL2_ADDR};

    if (HAL_UART_Receive_IT(&huart1, &g_rxByte, 1) != HAL_OK ||
        HAL_ADCEx_InjectedStart_IT(&hadc1) != HAL_OK ||  // сначала АЦП: готов к первому TRGO
        HAL_TIM_Base_Start(&htim2) != HAL_OK) {          // таймер сам запускает АЦП 75 раз/с
        Error_Handler();
    }

    g_lastTick = HAL_GetTick();
    g_app.start(g_lastTick);
}

extern "C" void app_step(void)
{
    RxItem rx;
    while (rxPop(rx)) {
        g_app.onByte(rx.ch, rx.error);
    }

    // Логика рассчитана на шаг 1 мс. Если итерация задержалась (например, на
    // выводе в UART), вызываем tick() за каждую пропущенную миллисекунду.
    const std::uint32_t now = HAL_GetTick();
    while (g_lastTick != now) {
        g_lastTick++;
        const bool pressed = HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET;  // кнопка на GND
        g_app.tick(g_lastTick, pressed);
    }

    __WFI();  // спим до прерывания; SysTick будит каждую 1 мс
}

// ---- колбэки HAL ------------------------------------------------------------------
// В HAL это __weak-функции на C. В .cpp обязательно extern "C": иначе имя
// декорируется и линкер молча оставит пустую weak-версию.

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1) {
        // При ошибке чётности/кадра/переполнения HAL заполняет ErrorCode ДО вызова
        // этого колбэка (см. HAL_UART_IRQHandler), а перезапуск приёма его обнулит.
        rxPush(g_rxByte, huart->ErrorCode != HAL_UART_ERROR_NONE);
        rxStart();
    }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    // Если HAL остановил приём из-за ошибки — сообщаем и запускаем снова.
    if (huart->Instance == USART1 && huart->RxState == HAL_UART_STATE_READY) {
        rxPush(0, true);
        rxStart();
    }
}

extern "C" void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        // Rank 1 = VREFINT, Rank 2 = датчик температуры (как в CubeMX).
        g_adc.push(static_cast<std::uint16_t>(HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1)),
                   static_cast<std::uint16_t>(HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2)));
    }
}
