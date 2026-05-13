

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


/* CAN-USB 어댑터(tty_device)를 지정 보레이트로 열고 fd를 반환합니다.
 * 반환값: fd (성공) / -1 (실패) */
int adapter_init(const char *tty_device, int baudrate);

/* CAN 표준 데이터 프레임을 시리얼로 전송합니다.
 * 프레임 포맷: AA | 0xC0|dlc | id_lo | id_hi | data... | 55
 * 반환값: write() 결과 (성공 시 실제 쓴 바이트 수, 실패 시 -1) */
int send_can_frame(int tty_fd, uint16_t id, uint8_t *data, uint8_t dlc);

/* 시리얼에서 바이트를 1개씩 읽어 CAN 프레임을 조립합니다.
 * static 버퍼로 호출 간 상태를 유지하므로 여러 번 호출해야 완성됩니다.
 * 반환값: 1 = 프레임 완성, 0 = 미완성(더 읽어야 함), -1 = 에러 */
int receive_can_frame(int tty_fd, CAN_Frame *out_frame);

#ifdef __cplusplus
}
#endif

#endif /* CANUSB_LIB_H */