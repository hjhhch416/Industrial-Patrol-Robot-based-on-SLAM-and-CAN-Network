/*
 * ws_client.c
 */
#include "ws_client.h"
#include "nav2_bridge.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jansson.h>

/* ── 내부 상태 ───────────────────────────────────────── */
static int         s_can_fd   = -1;
static const char *s_server_ip = NULL;
static int         s_port     = 5000;

static struct lws_context_creation_info s_info;
static struct lws_context              *s_context   = NULL;
static struct lws_client_connect_info   s_ccinfo    = {0};
static struct lws                      *s_global_wsi = NULL;

/* 웹으로 보낼 최신 센서 데이터 */
static F446Status_t s_f446    = {0};
static F429Status_t s_f429    = {0};
static int          s_red_led = 0;
static int          s_buzzer  = 0;
static int          s_mode    = 0;
static int          s_servo   = 90;

/* 모터 페이로드 */
static const uint8_t MOTOR_STOP[4]  = {0x00,0x00,0x00,0x00};
static const uint8_t MOTOR_MOVE[4]  = {0x01,0xF4,0x01,0xF4};
static const uint8_t MOTOR_BACK[4]  = {0xFE,0x0C,0xFE,0x0C};
static const uint8_t MOTOR_LEFT[4]  = {0x01,0xF4,0xFE,0x0C};
static const uint8_t MOTOR_RIGHT[4] = {0xFE,0x0C,0x01,0xF4};
static const uint8_t ON_VAL  = 0x01;
static const uint8_t OFF_VAL = 0x00;

/* ── WebSocket 프로토콜 전방 선언 ─────────────────────── */
static int callback_robot(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len);
static struct lws_protocols s_protocols[] = {
    { "robot-protocol", callback_robot, 0, 4096 },
    { NULL, NULL, 0, 0 }
};

/* ── 센서 데이터 갱신 ────────────────────────────────── */
void ws_client_set_status(const F446Status_t *f446,
                           const F429Status_t *f429,
                           int red_led, int buzzer,
                           int mode, int servo_angle)
{
    if (f446) s_f446 = *f446;
    if (f429) s_f429 = *f429;
    s_red_led = red_led;
    s_buzzer  = buzzer;
    s_mode    = mode;
    s_servo   = servo_angle;
}

/* ── 데이터 송신 ─────────────────────────────────────── */
static void send_robot_data(struct lws *wsi)
{
    if (!wsi) return;
    /* LWS_PRE: libwebsockets가 헤더를 앞에 쓰기 위해 예약하는 바이트 수.
     * 실제 페이로드는 buf[LWS_PRE]부터 써야 합니다. */
    unsigned char buf[LWS_PRE + 512];
    unsigned char *p = &buf[LWS_PRE];

    json_t *root = json_object();
    json_object_set_new(root, "motor_l",      json_integer((int)s_f446.speed_l));
    json_object_set_new(root, "motor_r",      json_integer((int)s_f446.speed_r));
    json_object_set_new(root, "camera_angle", json_integer((int)s_f446.srv_angle));
    json_object_set_new(root, "ultrasonic",   json_integer((int)s_f446.us_dist));
    json_object_set_new(root, "aeb",          json_integer((int)s_f446.aeb_flag));
    json_object_set_new(root, "lux",          json_integer((int)s_f429.lux));
    json_object_set_new(root, "light",        json_integer((int)s_f429.rgb_led));
    json_object_set_new(root, "red_led",      json_integer(s_red_led));
    json_object_set_new(root, "buzzer",       json_integer(s_buzzer));
    json_object_set_new(root, "mode",         json_integer(s_mode));
    json_object_set_new(root, "status",       json_integer(s_red_led));

    char *serialized = json_dumps(root, 0);
    int n = sprintf((char *)p, "%s", serialized);
    lws_write(wsi, p, n, LWS_WRITE_TEXT);
    free(serialized);
    json_decref(root);  // jansson 참조 카운트 해제 (메모리 누수 방지)
}

/* ── WebSocket 이벤트 콜백 ───────────────────────────── */
static int callback_robot(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len)
{
    switch (reason) {

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        s_global_wsi = wsi;
        printf("✅ [WS] 서버 접속 성공!\n");
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        printf("❌ [WS] 접속 종료 - 재접속 시도...\n");
        s_global_wsi = NULL;
        lws_client_connect_via_info(&s_ccinfo);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        fprintf(stderr, "⚠️  [WS] 연결 오류 - 재접속 시도...\n");
        s_global_wsi = NULL;
        lws_client_connect_via_info(&s_ccinfo);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        char cmd[512] = {0};
        size_t copy_len = len < (sizeof(cmd)-1) ? len : (sizeof(cmd)-1);
        memcpy(cmd, in, copy_len);
        cmd[copy_len] = '\0';

        RobotMode mode = (RobotMode)s_mode;

        /* 모드 전환 */
        if (strstr(cmd, "AUTO_MODE_ON")) {
            s_mode = MODE_AUTO;
            alarm_reset();
            static const uint8_t m = 0;
            can_send(s_can_fd, ID_F446_MOTOR, MOTOR_STOP, 4, "자율주행 전환 - 모터 정지");
            can_send(s_can_fd, ID_F429_MODE,  &m, 1, "F429 AUTO 모드 전환");
            printf(">>> [모드] 자율주행 시작\n");
        }
        else if (strstr(cmd, "AUTO_MODE_OFF")) {
            s_mode = MODE_MANUAL;
            alarm_reset();
            static const uint8_t m = 1;
            can_send(s_can_fd, ID_F446_MOTOR, MOTOR_STOP, 4, "수동 전환 - 모터 정지");
            can_send(s_can_fd, ID_F429_MODE,  &m, 1, "F429 MANUAL 모드 전환");
            printf(">>> [모드] 수동 조종 복귀\n");
        }

        /* 수동 모드 이동·서보 명령 */
        if (s_mode == MODE_MANUAL) {
            if      (strstr(cmd, "ROVER_FORWARD"))  can_send(s_can_fd, ID_F446_MOTOR, MOTOR_MOVE,  4, "모터 전진");
            else if (strstr(cmd, "ROVER_BACKWARD")) can_send(s_can_fd, ID_F446_MOTOR, MOTOR_BACK,  4, "모터 후진");
            else if (strstr(cmd, "ROVER_LEFT"))     can_send(s_can_fd, ID_F446_MOTOR, MOTOR_LEFT,  4, "모터 좌회전");
            else if (strstr(cmd, "ROVER_RIGHT"))    can_send(s_can_fd, ID_F446_MOTOR, MOTOR_RIGHT, 4, "모터 우회전");
            else if (strstr(cmd, "ROVER_STOP"))     can_send(s_can_fd, ID_F446_MOTOR, MOTOR_STOP,  4, "모터 정지");

            if (strstr(cmd, "CAM_RIGHT")) {
                int angle = s_servo + 15;
                if (angle <= 180) {
                    s_servo = angle;
                    uint8_t ang = (uint8_t)angle;
                    can_send(s_can_fd, ID_F446_SERVO, &ang, 1, "서보 우회전");
                    printf("  [서보] 각도: %d도\n", s_servo);
                }
            } else if (strstr(cmd, "CAM_LEFT")) {
                int angle = s_servo - 15;
                if (angle >= 0) {
                    s_servo = angle;
                    uint8_t ang = (uint8_t)angle;
                    can_send(s_can_fd, ID_F446_SERVO, &ang, 1, "서보 좌회전");
                    printf("  [서보] 각도: %d도\n", s_servo);
                }
            }
        }

        /* 조명 제어 (수동 모드에서만) */
        if (strstr(cmd, "LIGHT_ON") && s_mode == MODE_MANUAL) {
            uint8_t m = 1;
            can_send(s_can_fd, ID_F429_MODE, &m,      1, "F429 MANUAL(RGB ON)");
            can_send(s_can_fd, ID_F429_RGB,  &ON_VAL, 1, "RGB 조명 ON");
        } else if (strstr(cmd, "LIGHT_OFF") && s_mode == MODE_MANUAL) {
            uint8_t m = 1;
            can_send(s_can_fd, ID_F429_MODE, &m,       1, "F429 MANUAL(RGB OFF)");
            can_send(s_can_fd, ID_F429_RGB,  &OFF_VAL, 1, "RGB 조명 OFF");
        }

        /* 경보 수동 제어 */
        if (strstr(cmd, "SIREN_ON")) {
            s_red_led = 1; s_buzzer = 1;
            can_send(s_can_fd, ID_F429_BUZZER, &ON_VAL, 1, "부저 ON");
            can_send(s_can_fd, ID_F429_LED,    &ON_VAL, 1, "경고 LED ON");
        } else if (strstr(cmd, "SIREN_OFF")) {
            s_red_led = 0; s_buzzer = 0;
            can_send(s_can_fd, ID_F429_BUZZER, &OFF_VAL, 1, "부저 OFF");
            can_send(s_can_fd, ID_F429_LED,    &OFF_VAL, 1, "경고 LED OFF");
        }

        lws_callback_on_writable(wsi);
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        send_robot_data(wsi);
        break;

    default:
        break;
    }
    return 0;
}

/* ── 초기화 ──────────────────────────────────────────── */
void ws_client_init(const char *server_ip, int port, int can_fd)
{
    s_server_ip = server_ip;
    s_port      = port;
    s_can_fd    = can_fd;

    memset(&s_info, 0, sizeof(s_info));
    s_info.port      = CONTEXT_PORT_NO_LISTEN;
    s_info.protocols = s_protocols;
    s_context        = lws_create_context(&s_info);

    s_ccinfo.context  = s_context;
    s_ccinfo.address  = s_server_ip;
    s_ccinfo.port     = s_port;
    s_ccinfo.path     = "/";
    s_ccinfo.host     = s_ccinfo.address;
    s_ccinfo.origin   = s_ccinfo.address;
    s_ccinfo.protocol = s_protocols[0].name;
    lws_client_connect_via_info(&s_ccinfo);
}

void ws_client_service(void)
{
    lws_service(s_context, 0);
}

void ws_client_request_write(void)
{
    if (s_global_wsi) lws_callback_on_writable(s_global_wsi);
}

void ws_client_destroy(void)
{
    lws_context_destroy(s_context);
}
