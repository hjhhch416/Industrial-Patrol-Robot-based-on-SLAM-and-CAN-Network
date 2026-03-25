

#include "canusb_lib.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <asm/termbits.h>   // termios2, BOTHER — 비표준 보레이트 설정에 필요
#include <sys/ioctl.h>
#include <string.h>


int adapter_init(const char *tty_device, int baudrate) {
    int fd = open(tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    // O_RDWR    : 읽기+쓰기
    // O_NOCTTY  : 이 포트를 제어 터미널로 쓰지 않음
    // O_NONBLOCK: 논블로킹 (데이터 없으면 즉시 반환)
    if (fd == -1) return -1;

    struct termios2 tio;
    if (ioctl(fd, TCGETS2, &tio) == -1) return -1;
    // 현재 포트 설정을 tio에 읽어옴

    tio.c_cflag &= ~CBAUD;                  // 기존 보레이트 비트 클리어
    tio.c_cflag |= BOTHER | CS8 | CSTOPB;  // BOTHER: 직접 숫자 지정
                                            // CS8: 데이터 8비트
                                            // CSTOPB: 스탑비트 2개 (Waveshare 스펙)
    tio.c_iflag = IGNPAR;                   // 패리티 에러 무시
    tio.c_oflag = 0;                        // 출력 처리 없음
    tio.c_lflag = 0;                        // 로컬 모드 없음 (raw 모드)
    tio.c_ispeed = baudrate;                // 수신 속도 = 2,000,000 bps
    tio.c_ospeed = baudrate;                // 송신 속도 = 2,000,000 bps

    if (ioctl(fd, TCSETS2, &tio) == -1) return -1;
    // 변경된 설정을 포트에 적용

    return fd;  // 성공 시 파일 디스크립터 반환
}


int send_can_frame(int tty_fd, uint16_t id, uint8_t *data, uint8_t dlc) {
    unsigned char frame[13]; // 최대 크기: SOF(1)+타입(1)+ID(2)+데이터(8)+EOF(1) = 13
    int len = 0;

    frame[len++] = 0xAA;                        // SOF: 프레임 시작 마커
    frame[len++] = 0xC0 | (dlc & 0x0F);         // 0xC0=표준 데이터 프레임, 하위 4비트=DLC
    frame[len++] = (uint8_t)(id & 0xFF);         // CAN ID 하위 바이트
    frame[len++] = (uint8_t)((id >> 8) & 0xFF);  // CAN ID 상위 바이트
    for (int i = 0; i < dlc; i++)
        frame[len++] = data[i];                  // 페이로드 (최대 8바이트)
    frame[len++] = 0x55;                         // EOF: 프레임 종료 마커

    return write(tty_fd, frame, len);
    // 성공: 실제로 쓴 바이트 수 반환 / 실패: -1
}


int receive_can_frame(int tty_fd, CAN_Frame *out_frame) {
    static uint8_t rx_buf[13]; // 수신 중인 프레임 버퍼 (static: 호출 간 유지)
    static int     pos = 0;    // 현재까지 읽은 바이트 수 (static: 호출 간 유지)
    uint8_t byte;

    if (read(tty_fd, &byte, 1) > 0) {
        // 첫 번째 바이트가 SOF(0xAA)가 아니면 동기화 실패 → 버림
        if (pos == 0 && byte != 0xAA) return 0;

        rx_buf[pos++] = byte;  // 버퍼에 저장

        if (pos >= 2) {
            int dlc = rx_buf[1] & 0x0F;  // 타입 바이트 하위 4비트 = 데이터 길이
            if (pos == dlc + 5) {         // 프레임 완성 조건
                // ID 복원: 하위 바이트 | (상위 바이트 << 8)
                out_frame->id  = rx_buf[2] | (rx_buf[3] << 8);
                out_frame->dlc = dlc;
                memcpy(out_frame->data, &rx_buf[4], dlc); // 페이로드 복사
                pos = 0;   // 다음 프레임을 위해 초기화
                return 1;  // 프레임 완성
            }
        }
    }
    return 0; // 아직 미완성 (더 읽어야 함)
}