#include "alarm.h"
#include "nav2_bridge.h"
#include "util.h"
#include <stdio.h>
#include <time.h>

/* ── 내부 상태 변수 ───────────────────────────────────── */
static int s_can_fd           = -1;
static int s_udp_fd           = -1;
static int s_alarm_duration   = 3;
static int s_cooldown_sec     = 2;

/* 자율주행 경보 */
static RobotState       s_state       = STATE_NORMAL;
static struct timespec  s_alarm_start;
static long long        s_alarm_end_ms = 0;

/* 수동 경보 */
static int              s_manual_active    = 0;
static struct timespec  s_manual_start;
static long long        s_manual_end_ms    = 0;

/* 웹 표시용 */
static int s_red_led = 0;
static int s_buzzer  = 0;

/* ON/OFF 페이로드 */
static const uint8_t ON_VAL  = 0x01;
static const uint8_t OFF_VAL = 0x00;

/* ── 초기화 ──────────────────────────────────────────── */
void alarm_init(int can_fd, int udp_fd,
                int alarm_duration_sec, int cooldown_sec)
{
    s_can_fd         = can_fd;
    s_udp_fd         = udp_fd;
    s_alarm_duration = alarm_duration_sec;
    s_cooldown_sec   = cooldown_sec;
}

/* ── 경보 상태 초기화 (모드 전환 시 호출) ─────────────── */
void alarm_reset(void)
{
    s_state        = STATE_NORMAL;
    s_manual_active = 0;
    can_send(s_can_fd, ID_F429_BUZZER, &OFF_VAL, 1, "부저 OFF(초기화)");
    can_send(s_can_fd, ID_F429_LED,    &OFF_VAL, 1, "LED OFF(초기화)");
    s_red_led = 0;
    s_buzzer  = 0;
}

/* ── Getter ──────────────────────────────────────────── */
RobotState alarm_get_state(void)   { return s_state; }
int        alarm_get_red_led(void) { return s_red_led; }
int        alarm_get_buzzer(void)  { return s_buzzer; }

/* ── 태스크 4: 수동 모드 경보 ────────────────────────── */
void alarm_tick_manual(void)
{
    int received = recv_latest_udp(s_udp_fd);

    if (received != -1)
        printf("  [UDP] 수신값=%d  alarm_active=%d\n",
               received, s_manual_active);

    /* 쿨다운 체크: 경보 해제 직후 연속 재감지를 방지합니다.
     * YOLO 모델이 프레임 경계에서 깜빡이면 경보가 반복 발생하기 때문입니다. */
    long long now_ms = get_ms();
    int in_cooldown  = (s_manual_end_ms > 0 &&
                       (now_ms - s_manual_end_ms) < (s_cooldown_sec * 1000LL));

    /* 정상 → 경보 (쿨다운 중이면 무시)
     * 수동 모드에서는 운전자가 직접 조종하므로 모터는 건드리지 않고
     * 경고음·LED만 켭니다. (auto 모드와의 핵심 차이) */
    if (received == 1 && !s_manual_active && !in_cooldown) {
        s_manual_active = 1;
        clock_gettime(CLOCK_MONOTONIC, &s_manual_start);
        printf("\n>>> [수동/경보] 위험 감지! 경보 시작 (모터 유지)\n");
        can_send(s_can_fd, ID_F429_BUZZER, &ON_VAL, 1, "부저 ON");
        can_send(s_can_fd, ID_F429_LED,    &ON_VAL, 1, "LED ON");
        s_red_led = 1; s_buzzer = 1;
    } else if (received == 1 && in_cooldown) {
        printf("  [수동/쿨다운 중] 재감지 무시 (남은: %lldms)\n",
               (s_cooldown_sec * 1000LL) - (now_ms - s_manual_end_ms));
    }

    /* 경보 해제 조건: YOLO가 0을 보내면 즉시 해제, 아니면 타이머 만료 후 해제 */
    if (s_manual_active) {
        double elapsed    = elapsed_sec(s_manual_start);
        int    early_clear = (received == 0);  // YOLO가 위험 없음을 명시적으로 통보

        if (elapsed >= s_alarm_duration || early_clear) {
            s_manual_active  = 0;
            s_manual_end_ms  = get_ms();  // 이 시점부터 쿨다운 시작
            can_send(s_can_fd, ID_F429_BUZZER, &OFF_VAL, 1, "부저 OFF");
            can_send(s_can_fd, ID_F429_LED,    &OFF_VAL, 1, "LED OFF");
            s_red_led = 0; s_buzzer = 0;
            if (early_clear)
                printf(">>> [수동/경보] 위험 해제 감지 → 즉시 종료 (%.1f초 경과)\n", elapsed);
            else
                printf(">>> [수동/경보] 경보 종료 → 쿨다운 %d초 후 재감지 허용\n", s_cooldown_sec);
        }
    }
}

/* ── 태스크 5: 자율주행 모드 경보 ───────────────────── */
void alarm_tick_auto(void)
{
    int received = recv_latest_udp(s_udp_fd);

    /* 쿨다운 체크 */
    long long now_ms    = get_ms();
    int in_cooldown     = (s_alarm_end_ms > 0 &&
                          (now_ms - s_alarm_end_ms) < (s_cooldown_sec * 1000LL));

    /* 정상 → 경보
     * 자율주행 모드에서는 Nav2가 모터를 제어하므로 경보 시 모터도 강제 정지합니다.
     * (아래에서 STATE_ALARM이면 nav2_recv_and_send를 호출하지 않음) */
    if (received == 1 && s_state == STATE_NORMAL && !in_cooldown) {
        s_state = STATE_ALARM;
        clock_gettime(CLOCK_MONOTONIC, &s_alarm_start);
        printf("\n>>> [자율주행] 위험 감지! 정지 및 경보 발생\n");

        /* 자율주행: 모터도 즉시 정지 (수동 모드와의 핵심 차이) */
        static const uint8_t stop[4] = {0x00, 0x00, 0x00, 0x00};
        can_send(s_can_fd, ID_F446_MOTOR, stop,    4, "자율주행 모터 정지");
        can_send(s_can_fd, ID_F429_BUZZER, &ON_VAL, 1, "부저 ON");
        can_send(s_can_fd, ID_F429_LED,    &ON_VAL, 1, "LED ON");
        s_red_led = 1; s_buzzer = 1;

    } else if (received == 1 && in_cooldown) {
        printf("  [쿨다운 중] 재감지 무시 (남은: %lldms)\n",
               (s_cooldown_sec * 1000LL) - (now_ms - s_alarm_end_ms));
    }

    /* 경보 상태 처리 */
    if (s_state == STATE_ALARM) {
        double elapsed     = elapsed_sec(s_alarm_start);
        int    early_clear = (received == 0);

        if (elapsed >= s_alarm_duration || early_clear) {
            /* 경보 해제 */
            can_send(s_can_fd, ID_F429_BUZZER, &OFF_VAL, 1, "부저 OFF");
            can_send(s_can_fd, ID_F429_LED,    &OFF_VAL, 1, "LED OFF");
            s_red_led     = 0; s_buzzer = 0;
            s_state       = STATE_NORMAL;
            s_alarm_end_ms = get_ms();  // 쿨다운 시작
            if (early_clear)
                printf(">>> [자율주행] 위험 해제 → 경보 즉시 종료 (%.1f초 경과)\n", elapsed);
            else
                printf(">>> [자율주행] 경보 종료 → 쿨다운 %d초 후 복귀\n", s_cooldown_sec);
            nav2_recv_and_send();  // 해제 즉시 Nav2 명령 재개 (재개 지연 방지)
        } else {
            /* 경보 중: 1초마다 MOTOR_STOP을 재전송합니다.
             * STM32 측에서 명령이 일정 시간 안 오면 자체 복귀할 수 있기 때문입니다. */
            static long long last_stop_ms = 0;
            if (now_ms - last_stop_ms >= 1000) {
                last_stop_ms = now_ms;
                static const uint8_t stop[4] = {0x00, 0x00, 0x00, 0x00};
                can_send(s_can_fd, ID_F446_MOTOR, stop, 4, "모터 정지 유지");
                printf("  [경보 중] 남은 시간: %.0f초\n", s_alarm_duration - elapsed);
            }
        }
    }

    /* 정상 상태: Nav2 cmd_vel 실행 */
    if (s_state == STATE_NORMAL) {
        nav2_recv_and_send();
    }
}