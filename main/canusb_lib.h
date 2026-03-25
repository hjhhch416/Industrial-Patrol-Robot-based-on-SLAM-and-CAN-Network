

#ifndef CANUSB_LIB_H
#define CANUSB_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


typedef struct {
    uint16_t id;      // CAN 메시지 ID (11비트, 예: 0x200)
    uint8_t  dlc;     // 데이터 길이 (0~8)
    uint8_t  data[8]; // 페이로드
} CAN_Frame;


int adapter_init(const char *tty_device, int baudrate);


int send_can_frame(int tty_fd, uint16_t id, uint8_t *data, uint8_t dlc);


int receive_can_frame(int tty_fd, CAN_Frame *out_frame);

#ifdef __cplusplus
}
#endif

#endif /* CANUSB_LIB_H */