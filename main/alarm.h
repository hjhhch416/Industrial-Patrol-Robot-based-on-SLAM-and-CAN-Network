#ifndef ALARM_H
#define ALARM_H

#include "robot_can.h"

/* 경보 상태 */
typedef enum { STATE_NORMAL = 0, STATE_ALARM } RobotState;

/* 운전 모드 */
typedef enum { MODE_MANUAL = 0, MODE_AUTO } RobotMode;

/*
 * alarm_init()
 *   경보 모듈에 필요한 fd·설정값을 주입합니다.
 *   init_main()에서 한 번 호출합니다.
 */
void alarm_init(int can_fd, int udp_fd,
                int alarm_duration_sec, int cooldown_sec);

/* alarm_tick_manual() — 태스크 4: 수동 모드 경보 처리 */
void alarm_tick_manual(void);

/* alarm_tick_auto() — 태스크 5: 자율주행 모드 경보 처리 */
void alarm_tick_auto(void);

/* 모드 전환 시 경보 상태 초기화 */
void alarm_reset(void);

/* 현재 경보 상태 반환 */
RobotState alarm_get_state(void);
int        alarm_get_red_led(void);
int        alarm_get_buzzer(void);

#endif