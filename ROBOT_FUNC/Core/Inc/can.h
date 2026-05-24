#ifndef CAN_H
#define CAN_H

#include "main.h"     // HAL CAN 타입/매크로 전부 포함
#include <stdint.h>

// ── CAN 메시지 ID 정의 (라즈베리 파이 -> STM32) ────────
#define CAN_ID_RX_SERVO     0x100U
#define CAN_ID_RX_RGB       0x101U
#define CAN_ID_RX_BUZZER    0x102U
#define CAN_ID_RX_LED       0x103U
#define CAN_ID_RX_MODE      0x104U

// ── CAN 메시지 ID 정의 (STM32 -> 라즈베리 파이) ────────
#define CAN_ID_TX_STATUS    0x105U  // 상태 보고(조도 + RGB LED + Red LED + 부저)

// ── 수신 콜백 함수 포인터 타입 ───────────────────────
// ap.c에서 ID별로 직접 분기 처리할 수 있도록 ID와 원본 데이터를 넘겨줍니다.
typedef void (*can_rx_cb_t)(uint32_t id, const uint8_t *data, uint8_t dlc);

// ── 외부 노출 API ────────────────────────────────────
void can_comm_init(CAN_HandleTypeDef *hcan, can_rx_cb_t cb);
void can_comm_rx_handler(CAN_HandleTypeDef *hcan);

// 상태 송신 함수
void can_comm_tx_status(uint16_t light_val,
                        uint8_t rgb_led_state,
                        uint8_t red_led_state,
                        uint8_t buzzer_state);

#endif /* CAN_H */