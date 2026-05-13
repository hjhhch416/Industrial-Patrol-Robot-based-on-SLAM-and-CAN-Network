// ap.h
#ifndef AP_H_
#define AP_H_

#include "main.h"
#include "ap_def.h"

#include "stm32f4xx_hal.h"

// 핀맵 설정
// 1. 모터 (PWM / DIR)
#define MOTOR_PWM_TIM   (&htim3)
#define MOTOR_L_CH      TIM_CHANNEL_1
#define MOTOR_R_CH      TIM_CHANNEL_2
#define MOTOR_DIR_PORT  DIR1_rover_GPIO_Port
#define MOTOR_L_DIR_PIN DIR1_rover_Pin
#define MOTOR_R_DIR_PIN DIR2_rover_Pin

// 2. 초음파 센서 (ECHO / TRIG)
#define ECHO_TIM     (&htim2)
#define ECHO_CH      TIM_CHANNEL_1
#define TRIG_PORT    TRIG_ultrasonic_GPIO_Port
#define TRIG_PIN     TRIG_ultrasonic_Pin

void apInit(void);
void apMain(void);

// main.c의 HAL_TIM_IC_CaptureCallback에서 호출
void apUltrasonic_OnCapture(TIM_HandleTypeDef *htim);

#endif