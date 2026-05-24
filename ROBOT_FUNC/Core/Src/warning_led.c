#include "warning_led.h"

// === 너 보드 기준 경고 LED 핀 ===
#define WARN_LED_GPIO_Port   GPIOF
#define WARN_LED_Pin         GPIO_PIN_15

void WarningLed_Init(void)
{
    // GPIO는 CubeMX(MX_GPIO_Init)에서 이미 설정되어 있어야 함
    WarningLed_Off();
}

void WarningLed_On(void)
{
    HAL_GPIO_WritePin(WARN_LED_GPIO_Port, WARN_LED_Pin, GPIO_PIN_SET);
}

void WarningLed_Off(void)
{
    HAL_GPIO_WritePin(WARN_LED_GPIO_Port, WARN_LED_Pin, GPIO_PIN_RESET);
}

void WarningLed_Toggle(void)
{
    HAL_GPIO_TogglePin(WARN_LED_GPIO_Port, WARN_LED_Pin);
}

void WarningLed_BlinkTick(uint32_t period_ms, uint32_t now_ms)
{
    static uint32_t last = 0;

    if ((now_ms - last) >= period_ms) {
        last = now_ms;
        WarningLed_Toggle();
    }
}