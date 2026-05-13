// servomotor.c
#include "servomotor.h"

// SG90의 최소/최대 펄스 폭 (단위: us)
// 만약 모터가 180도까지 안 가거나 덜덜 떨리면 이 값을 500~2500 또는 1000~2000 사이로 미세 조정하세요.
#define SERVO_MIN_PULSE 500  // 0도일 때 약 0.5ms
#define SERVO_MAX_PULSE 2500 // 180도일 때 약 2.5ms

/**
 * @brief 서보 모터 초기화 및 PWM 시작
 */
void Servo_Init(Servo_TypeDef *servo, TIM_HandleTypeDef *htim, uint32_t channel) {
    servo->htim = htim;
    servo->channel = channel;
    
    // PWM 출력 시작
    HAL_TIM_PWM_Start(servo->htim, servo->channel);
}

/**
 * @brief 서보 모터 각도 설정 (0 ~ 180도)
 */
void Servo_SetAngle(Servo_TypeDef *servo, uint8_t angle) {
    if (angle > 180) {
        angle = 180; // 각도 제한 보호 로직
    }

    // 각도(0~180)를 PWM 펄스 폭(500~2500)으로 매핑 계산
    // 공식: (각도 * (최대펄스 - 최소펄스) / 180) + 최소펄스
    uint32_t pulse = SERVO_MIN_PULSE + ((uint32_t)angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180);

    // 계산된 펄스 폭을 타이머 비교 레지스터(CCR)에 적용
    __HAL_TIM_SET_COMPARE(servo->htim, servo->channel, pulse);
}