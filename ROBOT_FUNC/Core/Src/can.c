#include "can.h"
#include <stddef.h>
#include <stdio.h>

/* 내부 상태 */
static CAN_HandleTypeDef *s_hcan  = NULL;
static can_rx_cb_t        s_rx_cb = NULL;

/* ⚠️ 실제 사용하는 UART로 맞춰야 함 */
extern UART_HandleTypeDef huart3;

/* ================= CAN Filter 설정 ================= */
static void can_filter_init(CAN_HandleTypeDef *hcan)
{
    CAN_FilterTypeDef f = {0};

    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = 0x0000;
    f.FilterIdLow          = 0x0000;
    f.FilterMaskIdHigh     = 0x0000;
    f.FilterMaskIdLow      = 0x0000;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = ENABLE;

    HAL_CAN_ConfigFilter(hcan, &f);
}

/* ================= CAN 초기화 ================= */
void can_comm_init(CAN_HandleTypeDef *hcan, can_rx_cb_t cb)
{
    if (hcan == NULL) return;

    s_hcan  = hcan;
    s_rx_cb = cb;

    can_filter_init(hcan);

    HAL_CAN_Start(hcan);
    HAL_CAN_ActivateNotification(hcan,
        CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ================= CAN TX (센서 값 전송) ================= */
void can_comm_tx_status(uint16_t light_val,
                        uint8_t rgb_led_state,
                        uint8_t red_led_state,
                        uint8_t buzzer_state)
{
    if (s_hcan == NULL) return;

    CAN_TxHeaderTypeDef tx_hdr = {0};
    uint32_t tx_mailbox;
    uint8_t  tx_data[5];

    tx_hdr.StdId = CAN_ID_TX_STATUS;
    tx_hdr.IDE   = CAN_ID_STD;
    tx_hdr.RTR   = CAN_RTR_DATA;
    tx_hdr.DLC   = 5;
    tx_hdr.TransmitGlobalTime = DISABLE;

    tx_data[0] = (uint8_t)(light_val >> 8);
    tx_data[1] = (uint8_t)(light_val & 0xFF);
    tx_data[2] = rgb_led_state;
    tx_data[3] = red_led_state;
    tx_data[4] = buzzer_state;

    if (HAL_CAN_GetTxMailboxesFreeLevel(s_hcan) > 0)
    {
        HAL_CAN_AddTxMessage(s_hcan,
                             &tx_hdr,
                             tx_data,
                             &tx_mailbox);
    }
}

/* ================= CAN RX 처리 ================= */
void can_comm_rx_handler(CAN_HandleTypeDef *hcan)
{

    char servo_log[50];
    int len = snprintf(servo_log, sizeof(servo_log), "[CAN] RXRXRXRXRXRRXRXRXRXRXRX\r\n");
    HAL_UART_Transmit(&huart3, (uint8_t*)servo_log, len, 10);

    if (s_hcan == NULL || hcan->Instance != s_hcan->Instance)
        return;

    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan,
                             CAN_RX_FIFO0,
                             &hdr,
                             data) == HAL_OK)
    {
        if (s_rx_cb != NULL)
        {
            s_rx_cb(hdr.StdId, data, hdr.DLC);
        }
    }
}