#include "led.h"

// CubeMX가 생성한 TIM 핸들(이미 main.c 어딘가에 있음)
extern TIM_HandleTypeDef htim1;

// TIM1 Period = 999 기준 상한
#define LED_MAX_DUTY   (999U)

static inline uint16_t clamp_duty(uint16_t v)
{
    return (v > LED_MAX_DUTY) ? LED_MAX_DUTY : v;
}

void LED_Init(void)
{
    // PWM Start (CubeMX에서 TIM1 PWM 설정되어 있어야 함)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); // Red  (예: PE9)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // Green(예: PE11)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3); // Blue (예: PE13)

    LED_Off();
}

void LED_SetRgb(uint16_t r, uint16_t g, uint16_t b)
{
    r = clamp_duty(r);
    g = clamp_duty(g);
    b = clamp_duty(b);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, r);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, g);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, b);
}

void LED_Off(void)
{
    LED_SetRgb(0, 0, 0);
}

void LED_White(uint16_t intensity)
{
    intensity = clamp_duty(intensity);
    LED_SetRgb(intensity, intensity, intensity);
}