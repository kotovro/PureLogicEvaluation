// Связь logic::App с железом через HAL. Единственный файл, который знает
// и про HAL, и про логику.
//
// Модель: суперцикл + прерывания. Прерывания только складывают данные,
// вся логика — в основном цикле.
#include "app_main.h"

#include <atomic>
#include <cstring>

#include "logic/app.hpp"
#include "main.h"
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

    if (HAL_UART_Receive_IT(&huart1, &g_rxByte, 1) != HAL_OK) {
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
