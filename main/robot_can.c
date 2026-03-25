/*
 * robot_can.c  —  로봇 전용 CAN 프로토콜 레이어
 *
 * [구조 변경]
 *   Before: robot_can_recv() 하나가 F446+F429 버퍼 소진 + 파싱 모두 담당
 *   After : can_rx_poll()    — 버퍼 소진 + 내부 정적 구조체에 파싱 결과 저장
 *           can_recv_f446()  — s_f446 구조체를 out에 복사
 *           can_recv_f429()  — s_f429 구조체를 out에 복사
 *
 * 호출 순서 (통합Main.c 태스크 3):
 *   can_rx_poll(can_fd);       // 버퍼 소진 + 파싱
 *   can_recv_f446(&f446_data); // F446 결과 가져오기
 *   can_recv_f429(&f429_data); // F429 결과 가져오기
 */

#include "robot_can.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

/* ═══════════════════════════════════════════════════
 * 내부 정적 구조체
 *   can_rx_poll()이 파싱한 결과를 여기 저장합니다.
 *   can_recv_f446() / can_recv_f429()는 이 값을 복사해 반환합니다.
 * ═══════════════════════════════════════════════════ */
static F446Status_t s_f446 = {0};  // F446 최신 수신값
static F429Status_t s_f429 = {0};  // F429 최신 수신값

// 이번 poll 호출에서 각 MCU 데이터가 갱신됐는지 플래그
static int s_f446_updated = 0;
static int s_f429_updated = 0;

/* ═══════════════════════════════════════════════════
 * can_send()
 *   방향: RPi → STM32 (송신)
 *   send_can_frame() 래퍼. 실패 시 에러 로그 출력.
 * ═══════════════════════════════════════════════════ */
int can_send(int can_fd, uint32_t id,
             const uint8_t *data, uint8_t len,
             const char *desc)
{
    int ret = send_can_frame(can_fd, (uint16_t)id, (uint8_t *)data, len);
    if (ret <= 0)
        fprintf(stderr, "[CAN 에러] %s 전송 실패 (id=0x%X, ret=%d)\n",
                desc, id, ret);
    else
        printf("  [CAN 송신] %s\n", desc);
    return ret;
}

/* ═══════════════════════════════════════════════════
 * can_rx_poll()
 *   방향: STM32 → RPi (수신)
 *   CAN 버퍼를 최대 256회 소진하며 프레임을 파싱합니다.
 *   F446(0x200)과 F429(0x105) 모두 이 함수 하나에서 처리합니다.
 *
 *   [핵심 설계 이유]
 *   F446과 F429가 같은 버스를 공유하므로 버퍼를 한 번만 소진해야 합니다.
 *   can_recv_f446()과 can_recv_f429()가 각자 버퍼를 읽으면
 *   먼저 호출된 쪽이 상대방 데이터를 읽고 버리는 문제가 생깁니다.
 * ═══════════════════════════════════════════════════ */
int can_rx_poll(int can_fd)
{
    // 매 poll 호출 시작 시 갱신 플래그 초기화
    s_f446_updated = 0;
    s_f429_updated = 0;

    CAN_Frame frame;
    int ret;
    int count    = 0;
    int max_iter = 256;  // 무한루프 방지

    while (max_iter-- > 0) {
        ret = receive_can_frame(can_fd, &frame);

        if (ret < 0) break;  // 에러 → 루프 종료

        if (ret == 0) {
            // 프레임 미완성: 버퍼 잔여 바이트 확인
            int avail = 0;
            ioctl(can_fd, FIONREAD, &avail);
            if (avail <= 0) break;  // 더 읽을 데이터 없음
            continue;
        }

        // ret == 1: 프레임 완성 → ID별 파싱
        if (frame.id == ID_F446_STATUS && frame.dlc >= 8) {
            // F446 상태 파싱: 빅엔디언 복원
            s_f446.aeb_flag  =  frame.data[0];
            s_f446.srv_angle =  frame.data[1];
            s_f446.us_dist   = (uint16_t)((frame.data[2] << 8) | frame.data[3]);
            s_f446.speed_l   = (int16_t) ((frame.data[4] << 8) | frame.data[5]);
            s_f446.speed_r   = (int16_t) ((frame.data[6] << 8) | frame.data[7]);
            s_f446_updated   = 1;  // 갱신 플래그 ON
            printf("  [CAN 수신] F446  AEB=%d  서보=%d°  거리=%dcm  L=%d  R=%d\n",
                   s_f446.aeb_flag, s_f446.srv_angle, s_f446.us_dist,
                   s_f446.speed_l,  s_f446.speed_r);
            count++;
        }
        else if (frame.id == ID_F429_STATUS && frame.dlc >= 5) {
            // F429 상태 파싱: 조도(2바이트 빅엔디언) + LED/부저(1바이트씩)
            s_f429.lux     = (uint16_t)((frame.data[0] << 8) | frame.data[1]);
            s_f429.rgb_led =  frame.data[2];
            s_f429.red_led =  frame.data[3];
            s_f429.buzzer  =  frame.data[4];
            s_f429_updated = 1;  // 갱신 플래그 ON
            printf("  [CAN 수신] F429  조도=%d  RGB=%d  LED=%d  부저=%d\n",
                   s_f429.lux, s_f429.rgb_led, s_f429.red_led, s_f429.buzzer);
            count++;
        }
        // 그 외 ID는 무시
    }
    return count;
}

/* ═══════════════════════════════════════════════════
 * can_recv_f446()
 *   can_rx_poll() 이후 파싱된 F446 데이터를 out에 복사합니다.
 *   방향: F446 → RPi (수신)
 *   반환값: 1 = 이번 poll에서 갱신됨, 0 = 갱신 없음
 * ═══════════════════════════════════════════════════ */
int can_recv_f446(F446Status_t *out)
{
    if (!out) return 0;
    memcpy(out, &s_f446, sizeof(F446Status_t));
    return s_f446_updated;
}

/* ═══════════════════════════════════════════════════
 * can_recv_f429()
 *   can_rx_poll() 이후 파싱된 F429 데이터를 out에 복사합니다.
 *   방향: F429 → RPi (수신)
 *   반환값: 1 = 이번 poll에서 갱신됨, 0 = 갱신 없음
 * ═══════════════════════════════════════════════════ */
int can_recv_f429(F429Status_t *out)
{
    if (!out) return 0;
    memcpy(out, &s_f429, sizeof(F429Status_t));
    return s_f429_updated;
}