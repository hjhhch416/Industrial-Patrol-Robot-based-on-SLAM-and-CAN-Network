/*
 * ws_client.h
 * 역할: WebSocket 클라이언트 — 명령 수신 + 센서 데이터 송신
 * 원래 위치: 통합Main.c
 *   - send_robot_data()   (304~411줄)
 *   - callback_robot()    (414~574줄)
 *   - socket_init()       (580~596줄)
 */
#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <libwebsockets.h>
#include "robot_can.h"
#include "alarm.h"

/*
 * ws_client_init()
 *   WebSocket 컨텍스트 생성 + Node.js 서버 연결 시도.
 *   init_main()에서 호출합니다.
 *
 *   server_ip : Node.js 서버 IP
 *   port      : Node.js 서버 포트 (보통 5000)
 *   can_fd    : CAN 송신용 fd
 */
void ws_client_init(const char *server_ip, int port, int can_fd);

/*
 * ws_client_service()
 *   WebSocket 이벤트 처리 (논블로킹).
 *   메인 루프에서 매 사이클 호출합니다.
 */
void ws_client_service(void);

/*
 * ws_client_request_write()
 *   대시보드 갱신 예약. 메인 루프에서 매 사이클 호출합니다.
 */
void ws_client_request_write(void);

/*
 * ws_client_set_status()
 *   send_robot_data()에서 참조할 센서 데이터를 갱신합니다.
 *   메인 루프의 CAN 수신 후 호출합니다.
 */
void ws_client_set_status(const F446Status_t *f446,
                           const F429Status_t *f429,
                           int red_led, int buzzer,
                           int mode, int servo_angle);

/*
 * ws_client_destroy()
 *   WebSocket 컨텍스트 해제. 프로그램 종료 시 호출합니다.
 */
void ws_client_destroy(void);

#endif