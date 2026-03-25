/*
 * robot_can.h  —  로봇 전용 CAN 프로토콜 레이어
 *
 * [변경 이력]
 *   v1: robot_can_recv() — F446+F429를 하나의 함수에서 처리
 *   v2: 함수 분리 (네이밍 일관성 + 책임 분리)
 *       can_rx_poll()    — 버퍼 소진 + ID별 내부 파싱 (내부 함수)
 *       can_recv_f446()  — F446 파싱 결과 구조체 반환
 *       can_recv_f429()  — F429 파싱 결과 구조체 반환
 *
 * [송신/수신 네이밍 규칙]
 *   can_send()       : RPi → STM32  (송신)
 *   can_recv_f446()  : F446 → RPi  (수신, CAN 0x200)
 *   can_recv_f429()  : F429 → RPi  (수신, CAN 0x105)
 */

#ifndef ROBOT_CAN_H
#define ROBOT_CAN_H

#include <stdint.h>
#include "canusb_lib.h"

/* ═══════════════════════════════════════════════════
 * CAN ID 정의
 * ═══════════════════════════════════════════════════ */
// RPi → F446
#define ID_F446_MOTOR   0x100   // 모터 속도 명령: int16 L + int16 R (빅엔디언)
#define ID_F446_SERVO   0x300   // 서보 각도 명령: uint8 angle (0~180)
// F446 → RPi
#define ID_F446_STATUS  0x200   // F446 상태 보고 (8바이트, 10Hz)

// RPi → F429
#define ID_F429_RGB     0x101   // RGB 조명 ON/OFF
#define ID_F429_BUZZER  0x102   // 부저 ON/OFF
#define ID_F429_LED     0x103   // 경고 LED ON/OFF
#define ID_F429_MODE    0x104   // 모드 전환 (0=AUTO, 1=MANUAL)
// F429 → RPi
#define ID_F429_STATUS  0x105   // F429 상태 보고 (5바이트)

/* ═══════════════════════════════════════════════════
 * 수신 데이터 구조체
 *   can_recv_f446() / can_recv_f429() 의 반환 타입.
 *   기존 RobotCanStatus_t 하나를 MCU별로 분리.
 * ═══════════════════════════════════════════════════ */

/* F446 수신 데이터 (CAN 0x200) */
typedef struct {
    uint8_t  aeb_flag;   // 긴급제동 여부 (0=정상, 1=제동 중)
    uint8_t  srv_angle;  // 서보 현재 각도 (0~180)
    uint16_t us_dist;    // 초음파 거리 (cm)
    int16_t  speed_l;    // 왼쪽 모터 실제 속도
    int16_t  speed_r;    // 오른쪽 모터 실제 속도
} F446Status_t;

/* F429 수신 데이터 (CAN 0x105) */
typedef struct {
    uint16_t lux;        // 조도 센서값
    uint8_t  rgb_led;    // RGB LED 상태
    uint8_t  red_led;    // 경고 LED 상태
    uint8_t  buzzer;     // 부저 상태
} F429Status_t;

/* ═══════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════ */

/*
 * can_send()
 *   send_can_frame() 래퍼. 실패 시 stderr 에러 로그 출력.
 *   방향: RPi → STM32 (송신)
 */
int can_send(int can_fd, uint32_t id,
             const uint8_t *data, uint8_t len,
             const char *desc);

/*
 * can_rx_poll()
 *   CAN 버퍼를 최대 256회 소진하며 수신 프레임을 ID별로 파싱합니다.
 *   파싱 결과는 내부 정적 구조체(s_f446, s_f429)에 저장됩니다.
 *   can_recv_f446() / can_recv_f429() 호출 전에 반드시 먼저 호출해야 합니다.
 *
 *   [왜 버퍼 소진을 여기서 한 번만 하는가]
 *   F446(0x200)과 F429(0x105)는 같은 CAN 버스를 공유합니다.
 *   can_recv_f446()과 can_recv_f429()가 각자 버퍼를 소진하면
 *   먼저 호출된 함수가 상대방 데이터까지 읽고 버립니다.
 *   따라서 버퍼 소진은 poll()에서 한 번만 하고,
 *   두 recv 함수는 이미 파싱된 결과만 가져갑니다.
 *
 *   반환값: 파싱한 프레임 수 (0 = 이번 호출에 수신 없음)
 */
int can_rx_poll(int can_fd);

/*
 * can_recv_f446()
 *   can_rx_poll() 이후 파싱된 F446 데이터를 out에 복사합니다.
 *   방향: F446 → RPi (수신, CAN 0x200)
 *
 *   반환값: 1 = 이번 poll에서 F446 데이터 갱신됨, 0 = 갱신 없음
 */
int can_recv_f446(F446Status_t *out);

/*
 * can_recv_f429()
 *   can_rx_poll() 이후 파싱된 F429 데이터를 out에 복사합니다.
 *   방향: F429 → RPi (수신, CAN 0x105)
 *
 *   반환값: 1 = 이번 poll에서 F429 데이터 갱신됨, 0 = 갱신 없음
 */
int can_recv_f429(F429Status_t *out);

#endif /* ROBOT_CAN_H */