//motor.c
#include "motor.h"

// speed 값은 -pwm_max ~ +pwm_max 범위를 기대하지만, 상위 로직이 실수로 더 큰 값을 넣을 수 있음
// 따라서 절댓값(|speed|)을 취한 뒤 최대 pwm_max로 제한(clamp)
static inline uint32_t clamp_abs(int32_t v, uint32_t maxv)
{
  if (v < 0) v = -v;
  if ((uint32_t)v > maxv) return maxv;
  return (uint32_t)v;
}

// 정수 값을 [lo, hi] 범위로 제한
static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// "출력만" 적용하는 내부 함수, DIR 핀과 PWM CCR만 설정
static void motor_apply(motor_ch_t *m, int32_t speed)
{
  // ---- 1) 방향 결정 ----
  GPIO_PinState dir = (speed >= 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  // 방향 반전 옵션(dir_invert)이 켜져 있으면 논리를 뒤집음
  if (m->dir_invert) {
    dir = (dir == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  }

  // DIR 출력 적용
  HAL_GPIO_WritePin(m->dir_port, m->dir_pin, dir);

  // ---- 2) 듀티 계산 및 적용 ----
  uint32_t duty = clamp_abs(speed, m->pwm_max);
  __HAL_TIM_SET_COMPARE(m->htim, m->ch, duty);
}

// 모터 1채널 제어에 필요한 "타이머 채널(PWM)"과 "GPIO(DIR)" 정보를 저장하고 기본 상태(정지, DIR=0)로 초기화
void motor_init(motor_ch_t *m,
                TIM_HandleTypeDef *htim, uint32_t ch,
                GPIO_TypeDef *dir_port, uint16_t dir_pin,
                uint32_t pwm_max,
                uint8_t dir_invert)
{
  // ---- 설정 저장 ----
  m->htim = htim; // PWM 타이머 핸들
  m->ch = ch; // PWM 채널
  m->dir_port = dir_port; // DIR GPIO 포트
  m->dir_pin = dir_pin; // DIR GPIO 핀
  m->pwm_max = pwm_max; // 듀티 최대치(ARR)
  m->dir_invert = dir_invert; // 방향 반전 옵션

  // Ramp 상태 초기화
  m->cur_speed = 0;
  m->tgt_speed = 0;
  m->ramp_step = 50; // 기본값(원하면 ap에서 변경)

  // ---- 초기 출력 상태(정지) ----
  motor_apply(m, 0);

  // DIR 핀을 0으로 초기화
  HAL_GPIO_WritePin(m->dir_port, m->dir_pin, GPIO_PIN_RESET);
}

// 해당 타이머 채널의 PWM 출력을 시작
void motor_start(motor_ch_t *m)
{
  HAL_TIM_PWM_Start(m->htim, m->ch);
}

// 즉시 출력 반영(램프 없이 바로 적용), 주로 "긴급 정지(ESTOP)", "디버그", "캘리브레이션" 용도로 사용
void motor_set(motor_ch_t *m, int32_t speed)
{
  // 입력 범위 제한
  speed = clamp_i32(speed, -(int32_t)m->pwm_max, (int32_t)m->pwm_max);

  // 출력 적용
  motor_apply(m, speed);

  // ramp 상태 동기화
  m->cur_speed = speed;
  m->tgt_speed = speed;
}

// 좌/우(또는 2채널) 모터를 동시에 설정하는 편의 함수
void motor_set_lr(motor_ch_t *l, motor_ch_t *r, int32_t left, int32_t right)
{
  motor_set(l, left);
  motor_set(r, right);
}

// ===== Ramp 기능 (평상시 주행용) =====
// ramp 업데이트 1회당 최대 변화량 설정
void motor_set_ramp_step(motor_ch_t *m, uint32_t step)
{
  // step=0이면 변화가 멈추니 최소 1 이상 권장
  if (step == 0) step = 1;
  m->ramp_step = step;
}

// 목표 속도만 설정(출력은 바로 바뀌지 않음)
void motor_set_target(motor_ch_t *m, int32_t target)
{
  m->tgt_speed = clamp_i32(target, -(int32_t)m->pwm_max, (int32_t)m->pwm_max);
}

// 현재 속도(cur_speed)가 목표(tgt_speed)를 향해 ramp_step만큼만 이동, 이동 후 motor_apply()로 실제 출력 반영
void motor_update_ramp(motor_ch_t *m)
{
  int32_t cur = m->cur_speed;
  int32_t tgt = m->tgt_speed;
  int32_t step = (int32_t)m->ramp_step;

  if (cur < tgt) {
    cur += step;
    if (cur > tgt) cur = tgt;
  } else if (cur > tgt) {
    cur -= step;
    if (cur < tgt) cur = tgt;
  }

  // 상태 갱신
  m->cur_speed = cur;

  // 출력만 반영 (cur/tgt 상태는 유지)
  motor_apply(m, cur);
}

// 좌/우 목표를 한번에 설정
void motor_set_target_lr(motor_ch_t *l, motor_ch_t *r, int32_t left_tgt, int32_t right_tgt)
{
  motor_set_target(l, left_tgt);
  motor_set_target(r, right_tgt);
}

// 좌/우 ramp 업데이트를 한번에 수행
void motor_update_ramp_lr(motor_ch_t *l, motor_ch_t *r)
{
  motor_update_ramp(l);
  motor_update_ramp(r);
}