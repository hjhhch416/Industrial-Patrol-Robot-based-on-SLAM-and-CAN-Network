#ifndef LED_H
#define LED_H

#include "main.h"
#include <stdint.h>

// 0~999 (TIM1 Period=999 기준)
typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} RgbDuty;

/**
 * @brief RGB LED (TIM1 PWM) 초기 시작
 * @note  MX_TIM1_Init() 이후에 호출해야 함.
 */
void LED_Init(void);

/**
 * @brief RGB 값 세팅 (0~999)
 */
void LED_SetRgb(uint16_t r, uint16_t g, uint16_t b);

/**
 * @brief RGB 끄기
 */
void LED_Off(void);

/**
 * @brief 흰색 켜기 (R=G=B=max)
 * @param intensity 0~999
 */
void LED_White(uint16_t intensity);

#endif // LED_H