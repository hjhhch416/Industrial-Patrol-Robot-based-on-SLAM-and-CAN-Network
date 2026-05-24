#ifndef WARNING_LED_H
#define WARNING_LED_H

#include "main.h"
#include <stdint.h>

/**
 * @brief 경고 LED(GPIO) 초기화
 * @note  MX_GPIO_Init() 이후 호출
 */
void WarningLed_Init(void);

/**
 * @brief 경고 LED ON
 */
void WarningLed_On(void);

/**
 * @brief 경고 LED OFF
 */
void WarningLed_Off(void);

/**
 * @brief 경고 LED 토글
 */
void WarningLed_Toggle(void);

/**
 * @brief 깜빡임(비차단). 주기적으로 호출하면 토글됨.
 * @param period_ms 깜빡임 주기(ms) 예: 500
 * @param now_ms 현재 tick(ms). HAL_GetTick() 넣으면 됨.
 */
void WarningLed_BlinkTick(uint32_t period_ms, uint32_t now_ms);

#endif // WARNING_LED_H