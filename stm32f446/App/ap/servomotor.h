// servomotor.h
#ifndef SERVOMOTOR_H
#define SERVOMOTOR_H

#include "main.h"

// 서보 모터 구조체 선언
typedef struct {
    TIM_HandleTypeDef *htim; // 타이머 핸들
    uint32_t channel;        // 타이머 채널
} Servo_TypeDef;

// 함수 프로토타입
void Servo_Init(Servo_TypeDef *servo, TIM_HandleTypeDef *htim, uint32_t channel);
void Servo_SetAngle(Servo_TypeDef *servo, uint8_t angle);

#endif