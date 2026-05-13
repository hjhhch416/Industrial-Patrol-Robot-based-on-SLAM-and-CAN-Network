
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "canusb_lib.h"
#include "robot_can.h"
#include "util.h"
#include "nav2_bridge.h"
#include "alarm.h"
#include "ws_client.h"
#include "yolo_thread.h"

// ── 설정 ──────────────────────────────────────────────────
#define UDP_PORT           9999
#define UDP_CMDVEL_PORT    10000
#define ALARM_DURATION_SEC 3
#define ALARM_COOLDOWN_SEC 2
#define LOOP_INTERVAL_US   100000   // 100ms
#define CAN_RX_INTERVAL_MS 100
#define MAX_MOTOR_VAL      500
#define SERVO_CENTER       90
#define SERVER_IP          "192.168.0.48"
#define SERVER_PORT        5000

// ── fd ────────────────────────────────────────────────────
static int can_fd    = -1;
static int udp_fd    = -1;
static int cmdvel_fd = -1;

// ── CAN 수신 타이밍 ───────────────────────────────────────
static long long last_can_rx_ms = 0;


// ── 초기화 ────────────────────────────────────────────────
static int init_main(void)
{
    // 1. CAN 어댑터
    can_fd = adapter_init("/dev/ttyUSB0", 2000000);
    if (can_fd < 0) { perror("[에러] CAN 어댑터 초기화 실패"); return -1; }
    printf("[*] CAN 어댑터 초기화 완료\n");

    // 2. UDP 소켓
    udp_fd    = create_udp_socket(UDP_PORT);
    if (udp_fd    < 0) return -1;
    cmdvel_fd = create_udp_socket(UDP_CMDVEL_PORT);
    if (cmdvel_fd < 0) return -1;

    // 3. 서보 초기 위치 (90도)
    uint8_t init_ang = SERVO_CENTER;
    can_send(can_fd, ID_F446_SERVO, &init_ang, 1, "서보 초기화");

    // 4. F429 초기 모드 (MANUAL)
    uint8_t init_mode = 1;
    can_send(can_fd, ID_F429_MODE, &init_mode, 1, "F429 초기 MANUAL 설정");

    // 5. 각 모듈 초기화
    nav2_bridge_init(cmdvel_fd, can_fd, MAX_MOTOR_VAL);
    alarm_init(can_fd, udp_fd, ALARM_DURATION_SEC, ALARM_COOLDOWN_SEC);
    ws_client_init(SERVER_IP, SERVER_PORT, can_fd);

    // 6. YOLO 스레드 시작
    if (yolo_thread_start() < 0) return -1;

    printf("[*] 로봇 제어 시스템 가동 중\n");
    return 0;
}

// ── 메인 루프 ─────────────────────────────────────────────
int main(void)
{
    if (init_main() < 0) return -1;

    while (1) {
        // 태스크 1: WebSocket 이벤트 처리 (논블로킹, timeout=0)
        ws_client_service();

        // 태스크 2: 대시보드 갱신 예약 (다음 WRITEABLE 콜백에서 JSON 전송)
        ws_client_request_write();

        // 태스크 3: CAN 수신 (100ms 간격)
        // WebSocket보다 낮은 빈도로 실행해 CAN 버스 부하를 줄입니다.
        {
            long long now_ms = get_ms();
            if (now_ms - last_can_rx_ms >= CAN_RX_INTERVAL_MS) {
                last_can_rx_ms = now_ms;

                can_rx_poll(can_fd);    // 버퍼를 한 번에 소진해 F446·F429 모두 파싱

                F446Status_t f446 = {0};
                F429Status_t f429 = {0};
                can_recv_f446(&f446);   // poll에서 저장된 결과를 복사
                can_recv_f429(&f429);

                ws_client_set_status(&f446, &f429,
                                     alarm_get_red_led(),
                                     alarm_get_buzzer(),
                                     (int)alarm_get_state(),
                                     SERVO_CENTER);
            }
        }

        /* 태스크 4·5는 매 루프 모두 호출됩니다.
         * 내부에서 s_mode/s_state를 체크해 활성 상태일 때만 동작합니다. */

        // 태스크 4: 수동 모드 경보 (모터 정지 없이 부저·LED만 제어)
        alarm_tick_manual();

        // 태스크 5: 자율주행 모드 경보 + Nav2 cmd_vel 전달
        alarm_tick_auto();

        usleep(LOOP_INTERVAL_US);  // 100ms 슬립 (CPU 점유 방지)
    }

    // 정리 (while(1)로 도달 불가, SIGINT 처리 추가 시 필요)
    close(udp_fd);
    close(cmdvel_fd);
    close(can_fd);
    ws_client_destroy();
    return 0;
}