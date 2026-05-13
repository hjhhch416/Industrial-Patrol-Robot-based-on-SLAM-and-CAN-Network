#include "can_comm.h"
#include <string.h>

// ── 내부 상태 ─────────────────────────────────────────
static CAN_HandleTypeDef *s_hcan = NULL;
static can_motor_cmd_cb_t s_motor_cb = NULL;
static can_servo_cmd_cb_t s_servo_cb = NULL;

// ── CAN 필터 (전체 ID 수신) ───────────────────────────
static void can_filter_init(CAN_HandleTypeDef *hcan) {
  CAN_FilterTypeDef f = {0};
  f.FilterBank = 0;
  f.FilterMode = CAN_FILTERMODE_IDMASK;
  f.FilterScale = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh = 0x0000;
  f.FilterIdLow = 0x0000;
  f.FilterMaskIdHigh = 0x0000;
  f.FilterMaskIdLow = 0x0000;
  f.FilterFIFOAssignment = CAN_RX_FIFO0;
  f.FilterActivation = ENABLE;
  HAL_CAN_ConfigFilter(hcan, &f);
}

// ── 초기화 ────────────────────────────────────────────
void can_comm_init(CAN_HandleTypeDef *hcan, can_motor_cmd_cb_t m_cb,
                   can_servo_cmd_cb_t s_cb) {
  s_hcan = hcan;
  s_motor_cb = m_cb;
  s_servo_cb = s_cb;

  HAL_CAN_Start(hcan);
  HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_ERROR);
  can_filter_init(hcan);
}

// ── CAN 재시작 (Bus-Off 복구 / B1 버튼 수동 복구) ─────
void can_comm_restart(void) {
  if (s_hcan == NULL) return;
  HAL_CAN_Stop(s_hcan);
  HAL_CAN_Start(s_hcan);
  HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_ERROR);
  can_filter_init(s_hcan);
}

// ── 수신 처리 ─────────────────────────────────────────
void can_comm_rx_handler(CAN_HandleTypeDef *hcan) {
  if (hcan->Instance != s_hcan->Instance)
    return;

  CAN_RxHeaderTypeDef hdr;
  uint8_t data[8] = {0};

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hdr, data) != HAL_OK)
    return;

  // 1. 바퀴 모터 명령 (ID: 0x100)
  if (hdr.StdId == CAN_ID_MOTOR_CMD && hdr.DLC >= 4) {
    if (s_motor_cb != NULL) {
      can_motor_cmd_t cmd;
      cmd.left = (int16_t)((data[0] << 8) | data[1]);
      cmd.right = (int16_t)((data[2] << 8) | data[3]);
      s_motor_cb(&cmd);
    }
  }
  //  2. 서보 모터 각도 명령 (ID: 0x300, 1바이트)
  else if (hdr.StdId == CAN_ID_SERVO_CMD && hdr.DLC >= 1) {
    if (s_servo_cb != NULL) {
      can_servo_cmd_t cmd;
      cmd.angle = data[0]; // 첫 번째 칸만 읽습니다
      s_servo_cb(&cmd);
    }
  }
}

// ── 송신 처리 (상태를 라파로 전송) ───────────────────────────
void can_comm_tx_status(uint8_t aeb_flag, uint8_t s_angle, uint16_t us_dist,
                        int16_t speed_l, int16_t speed_r) {
  if (s_hcan == NULL)
    return;
  if (HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) == 0)
    return;

  CAN_TxHeaderTypeDef tx_hdr;
  uint32_t tx_mailbox;
  uint8_t tx_data[8] = {0};

  tx_hdr.StdId = CAN_ID_STATUS_TX;
  tx_hdr.ExtId = 0x00;
  tx_hdr.RTR = CAN_RTR_DATA;
  tx_hdr.IDE = CAN_ID_STD;
  tx_hdr.DLC = 8;
  tx_hdr.TransmitGlobalTime = DISABLE;

  // Data 구조 합의:
  // [0] : 긴급 제동 여부 플래그 (0: 정상, 1: 긴급 제동 중)
  // [1] : 서보 모터 현재 목표 각도 (0~180)
  // [2] : 초음파 센서 거리 상위 바이트
  // [3] : 초음파 센서 거리 하위 바이트
  // [4] : 왼쪽 모터 속도 상위 바이트
  // [5] : 왼쪽 모터 속도 하위 바이트
  // [6] : 오른쪽 모터 속도 상위 바이트
  // [7] : 오른쪽 모터 속도 하위 바이트

  tx_data[0] = aeb_flag;
  tx_data[1] = s_angle;
  tx_data[2] = (uint8_t)(us_dist >> 8); // 0x0a28 : 26.00cm  0x0a 0x28
  tx_data[3] = (uint8_t)(us_dist & 0xFF);
  tx_data[4] = (uint8_t)(speed_l >> 8);
  tx_data[5] = (uint8_t)(speed_l & 0xFF);
  tx_data[6] = (uint8_t)(speed_r >> 8);
  tx_data[7] = (uint8_t)(speed_r & 0xFF);

  HAL_CAN_AddTxMessage(s_hcan, &tx_hdr, tx_data, &tx_mailbox); 
}