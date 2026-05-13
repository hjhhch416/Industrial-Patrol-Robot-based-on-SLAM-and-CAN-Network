//motor.h
#ifndef MOTOR_H
#define MOTOR_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
  TIM_HandleTypeDef *htim; // PWM을 출력하는 타이머 핸들 (예: &htim3)
  uint32_t ch;             // PWM 채널 (예: TIM_CHANNEL_1)
  GPIO_TypeDef *dir_port;  // DIR 신호 GPIO 포트 (예: GPIOA)
  uint16_t dir_pin;        // DIR 신호 GPIO 핀 (예: GPIO_PIN_8)
  uint32_t pwm_max;        // PWM 최대값(=타이머 ARR). speed의 절댓값 clamp 기준
  uint8_t dir_invert;      // 방향 반전 옵션, 방향 반대면 1

  // ===== Ramp(가속 제한)용 상태 =====
  int32_t cur_speed;  // 현재 적용 중인 speed (-pwm_max ~ +pwm_max)
  int32_t tgt_speed;  // 목표 speed
  uint32_t ramp_step; // update 1회당 최대 변화량 (예: 30~80)
} motor_ch_t;

void motor_init(motor_ch_t *m, TIM_HandleTypeDef *htim, uint32_t ch,
                GPIO_TypeDef *dir_port, uint16_t dir_pin, uint32_t pwm_max,
                uint8_t dir_invert);

// HAL_TIM_PWM_Start()를 호출하여 해당 채널의 PWM 출력을 시작
void motor_start(motor_ch_t *m);

// 즉시 출력 반영(램프 없이 바로 적용)
void motor_set(motor_ch_t *m, int32_t speed); // -pwm_max ~ +pwm_max

// 좌/우(또는 2채널) 모터를 한 번에 설정하는 편의 함수
void motor_set_lr(motor_ch_t *l, motor_ch_t *r, int32_t left, int32_t right);

// 목표 설정 후 주기적으로 update를 호출하면 서서히 가감속됨(평상시 주행용)
void motor_set_ramp_step(motor_ch_t *m, uint32_t step);
void motor_set_target(motor_ch_t *m, int32_t target);
void motor_update_ramp(motor_ch_t *m); // cur_speed가 tgt_speed로 step만큼 접근

void motor_set_target_lr(motor_ch_t *l, motor_ch_t *r, int32_t left_tgt, int32_t right_tgt);
void motor_update_ramp_lr(motor_ch_t *l, motor_ch_t *r);

#endif

/*
 * [motor 모듈 사용 흐름 - 10줄 요약]
 * 1) CubeMX에서 PWM 타이머/채널(TIMx_CHy)과 DIR GPIO 핀을 설정(ARR=PWM_MAX 값 확인)
 * 2) apInit()에서 motor_init(&mL, &htim3, TIM_CHANNEL_1, DIR_PORT, DIR_PIN, PWM_MAX, dir_invert) 1회 호출
 * 3) apInit()에서 motor_start(&mL)로 PWM 출력 시작(좌/우 채널 각각)
 * 4) (평상시 권장) motor_set_ramp_step(&mL, step)로 가속/감속 속도(1회 변화량) 설정
 * 5) 평상시 주행은 "즉시 motor_set()" 대신, motor_set_target(&mL, target_speed)로 목표만 설정
 * 6) 메인 루프에서 주기적으로(예: 10~20ms) motor_update_ramp(&mL)을 호출해 cur_speed를 tgt로 서서히 이동
 * 7) 좌/우 동시 제어는 motor_set_target_lr(&mL,&mR, Ltgt, Rtgt) + motor_update_ramp_lr(&mL,&mR) 사용
 * 8) 긴급정지(ESTOP/디버그)는 motor_set(&mL, 0) 또는 motor_set_lr(&mL,&mR,0,0)로 즉시 0 출력
 * 9) 방향이 반대로 느껴지면 dir_invert=1로 보정하거나 모터 단자(A/B) 교체로 보정
 * 10) 최종적으로 ap에서는 "cmd_raw → safe_gate → (RUN:ramp / ESTOP:즉시0)" 순서로 motor에 적용하면 안정적
 */