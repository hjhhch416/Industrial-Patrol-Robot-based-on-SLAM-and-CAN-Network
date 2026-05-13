#include "ap.h"
#include "can_comm.h"
#include "motor.h"
#include "safe_distance.h"
#include "servomotor.h"
#include "ultrasonic.h"

#include <stdint.h>

// CubeMX 생성 핸들 (UART 제외됨)
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

// ==============================
// 객체 선언
// ==============================
static motor_ch_t mL;
static motor_ch_t mR;
static ultrasonic_t us;
static safe_distance_t sd;
Servo_TypeDef myServo;

// ==============================
// 상태 및 목표값 변수
// ==============================
static uint8_t target_angle = 90; // 서보 목표 각도 (초기값 정면)

// 주기 제어용 타이머
static uint32_t last_loop_ms = 0;
static uint32_t last_trig_ms = 0;
static uint32_t last_tx_ms = 0;

// CAN 수신 데이터 및 안전 속도 저장용
static int32_t g_can_l_speed = 0;
static int32_t g_can_r_speed = 0;
static int32_t current_safe_speed_l = 0;
static int32_t current_safe_speed_r = 0;

// -------------------------------------------------
// CAN 콜백 함수
// -------------------------------------------------
// 1. 바퀴 모터 수신 콜백 (ID: 0x100)
void on_can_motor_cmd(const can_motor_cmd_t *cmd) {
  g_can_l_speed = cmd->left;
  g_can_r_speed = cmd->right;
  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); // 수신 확인용 LED
}

// 2. 서보 모터 수신 콜백 (ID: 0x101)
void on_can_servo_cmd(const can_servo_cmd_t *cmd) {
  if (cmd->angle <= 180) {
    target_angle = cmd->angle; // 수신 즉시 목표 각도 변경
  }
}

// =====================================================
// 초기화 함수
// =====================================================
void apInit(void) {
  uint32_t pwm_max = __HAL_TIM_GET_AUTORELOAD(&htim3);

  // 모터 및 안전거리 초기화
  motor_init(&mL, MOTOR_PWM_TIM, MOTOR_L_CH, MOTOR_DIR_PORT, MOTOR_L_DIR_PIN,
             pwm_max, 0);
  motor_init(&mR, MOTOR_PWM_TIM, MOTOR_R_CH, MOTOR_DIR_PORT, MOTOR_R_DIR_PIN,
             pwm_max, 1);
  motor_start(&mL);
  motor_start(&mR);
  motor_set_ramp_step(&mL, 10);
  motor_set_ramp_step(&mR, 10);

  ultrasonic_init(&us, &htim2, ECHO_CH, TRIG_PORT, TRIG_PIN, 50);
  safe_distance_init(&sd, 15, 20, 2, 2, 200);
  // 120(2cm), 25000(약 4m), 8192(0.25 비율 필터링)
  safe_distance_set_filter(&sd, 120, 25000, 8192);
  
  motor_set_lr(&mL, &mR, 0, 0);

  // 서보 모터 초기화
  Servo_Init(&myServo, &htim4, TIM_CHANNEL_1);
  Servo_SetAngle(&myServo, target_angle);

  // CAN 통신 초기화 및 콜백 등록
  extern CAN_HandleTypeDef hcan1;
  can_comm_init(&hcan1, on_can_motor_cmd, on_can_servo_cmd);
}

// =====================================================
// 메인 루프
// =====================================================
void apMain(void) {
  uint32_t now_ms = HAL_GetTick();

  // 1) 서보 각도 적용 (목표 각도가 바뀌었을 때만 1회 적용)
  static uint8_t last_angle = 255;
  if (target_angle != last_angle) {
    Servo_SetAngle(&myServo, target_angle);
    last_angle = target_angle;
  }

  // 2) 초음파 센서 트리거 (60ms 간격)
  if (now_ms - last_trig_ms >= 60) {
    last_trig_ms = now_ms;
    ultrasonic_trigger(&us, now_ms);
  }

  // 3) 로버 메인 제어 루프 (10ms 간격)
  if (now_ms - last_loop_ms >= 10) {
    last_loop_ms = now_ms;

    if (ultrasonic_poll(&us, now_ms) == US_OK) {
      uint32_t echo = ultrasonic_echo_us(&us);
      safe_distance_feed_echo_us(&sd, now_ms, echo);
    }

    int32_t l_tgt_raw = g_can_l_speed;
    int32_t r_tgt_raw = g_can_r_speed;
    int32_t l_tgt_safe = l_tgt_raw;
    int32_t r_tgt_safe = r_tgt_raw;

    // 후진 시 안전 로직 적용
    if (l_tgt_raw < 0 && r_tgt_raw < 0) {
      safe_distance_apply_cmd(&sd, l_tgt_raw, r_tgt_raw, now_ms, &l_tgt_safe,
                              &r_tgt_safe);
    }

    // 최종 제어값 저장 및 모터 구동
    current_safe_speed_l = l_tgt_safe;
    current_safe_speed_r = r_tgt_safe;
    motor_set_target(&mL, l_tgt_safe);
    motor_set_target(&mR, r_tgt_safe);
    motor_update_ramp(&mL);
    motor_update_ramp(&mR);
  }

  // 4) CAN 상태 송신 루프 (1000ms 간격 = 1Hz)
  if (now_ms - last_tx_ms >= 1000) {
    last_tx_ms = now_ms;

    uint8_t is_aeb = 0;
    // 후진 명령이 들어왔는데 속도가 0으로 강제 하향되었다면 긴급정지 상태!
    if ((g_can_l_speed < 0 && g_can_r_speed < 0) &&
        (current_safe_speed_l == 0 && current_safe_speed_r == 0)) {
      is_aeb = 1;
    }

    // 라즈베리파이로 8바이트 상태 송신 (ID: 0x200)
    uint16_t current_us_dist = ultrasonic_echo_us(&us) / 58;
    int16_t speed_l = (int16_t)current_safe_speed_l;
    int16_t speed_r = (int16_t)current_safe_speed_r;

    can_comm_tx_status(is_aeb, target_angle, current_us_dist, speed_l, speed_r);
  }
}

// =====================================================
// 타이머 인터럽트 콜백 (초음파 에코 수신용)
// =====================================================
void apUltrasonic_OnCapture(TIM_HandleTypeDef *htim) {
  ultrasonic_on_capture_irq(&us, htim);
}