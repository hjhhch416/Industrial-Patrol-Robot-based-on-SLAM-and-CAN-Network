#ifndef CAN_COMM_H_
#define CAN_COMM_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

// ── CAN 메시지 ID 정의 ────────────────────────────────
#define CAN_ID_MOTOR_CMD 0x100U // 라파 → STM32 : 바퀴 모터 제어 명령
#define CAN_ID_SERVO_CMD 0x300U // 라파 → STM32 : 서보 모터 제어 명령
#define CAN_ID_STATUS_TX 0x200U // STM32 → 라파 : 상태 보고

// ── 모터 명령 데이터 구조 (0x100) ─────────────────────
// Data[0~1] : Left  speed
// Data[2~3] : Right speed
typedef struct {
  int16_t left;
  int16_t right;
} can_motor_cmd_t;

// ── 서보 모터 명령 데이터 구조 (0x101) ─────────────────
// Data[0] : 서보 모터 목표 각도 (0~180)
typedef struct {
  uint8_t angle;
} can_servo_cmd_t;

// ── 수신 콜백 등록 타입 ───────────────────────────────
typedef void (*can_motor_cmd_cb_t)(const can_motor_cmd_t *cmd);
typedef void (*can_servo_cmd_cb_t)(
    const can_servo_cmd_t *cmd); // 서보 콜백 추가

// ── API ──────────────────────────────────────────────
// 초기화 시 콜백 함수를 2개 모두 받도록 변경됨
void can_comm_init(CAN_HandleTypeDef *hcan, can_motor_cmd_cb_t m_cb,
                   can_servo_cmd_cb_t s_cb);
void can_comm_rx_handler(CAN_HandleTypeDef *hcan);
void can_comm_restart(void);

// 상태 송신(TX) 함수
void can_comm_tx_status(uint8_t aeb_flag, uint8_t s_angle, uint16_t us_dist,
                        int16_t speed_l, int16_t speed_r);

#endif /* CAN_COMM_H_ */