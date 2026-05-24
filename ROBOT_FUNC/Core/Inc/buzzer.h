#ifndef BUZZER_H
#define BUZZER_H

#include "main.h"
#include <stdint.h>

/**
 * @brief 부저 초기화 (PWM Start)
 * @note  MX_TIM3_Init() 이후 호출
 */
void Buzzer_Init(void);

/**
 * @brief 부저 ON (duty 0~999). 단순 ON/OFF용이면 0 또는 500 같은 값 사용.
 */
void Buzzer_On(uint16_t duty);

/**
 * @brief 부저 OFF
 */
void Buzzer_Off(void);

/**
 * @brief "삑!" (비차단) - 주기적으로 호출하면 duration 동안만 울림
 * @param duty 0~999
 * @param duration_ms 울리는 시간
 * @param now_ms HAL_GetTick()
 */
void Buzzer_BeepTick(uint16_t duty, uint32_t duration_ms, uint32_t now_ms);

#endif