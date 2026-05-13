
#include "nav2_bridge.h"
#include "robot_can.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

static int s_cmdvel_fd  = -1;
static int s_can_fd     = -1;
static int s_max_motor  = 500;

void nav2_bridge_init(int cmdvel_fd, int can_fd, int max_motor_val)
{
    s_cmdvel_fd = cmdvel_fd;
    s_can_fd    = can_fd;
    s_max_motor = max_motor_val;
}

void nav2_recv_and_send(void)
{
    char buf[16], latest[16] = {0};
    ssize_t n;

    /* UDP 버퍼를 전부 비우고 마지막(최신) 패킷만 사용합니다.
     * Nav2는 10Hz로 cmd_vel을 전송하는데 메인 루프(100ms)보다 빠를 수 있어
     * 오래된 패킷을 그대로 쓰면 이미 지난 명령이 모터에 전달됩니다. */
    while ((n = recvfrom(s_cmdvel_fd, buf, sizeof(buf)-1, 0, NULL, NULL)) > 0) {
        buf[n] = '\0';
        strncpy(latest, buf, sizeof(latest)-1);
    }
    if (latest[0] == '\0' || strlen(latest) < 8) return;

    /* UDP 패킷 포맷: "LLLLRRRR" (16진수 ASCII 8자리)
     * 예: "01F4FE0C" → L=+500, R=-500 (int16_t 해석) */
    char l_str[5] = {0}, r_str[5] = {0};
    strncpy(l_str, latest,     4);
    strncpy(r_str, latest + 4, 4);

    int16_t l = (int16_t)(int)strtol(l_str, NULL, 16);
    int16_t r = (int16_t)(int)strtol(r_str, NULL, 16);

    /* ±MAX_MOTOR_VAL 클램핑 */
    if (l >  s_max_motor) l =  s_max_motor;
    if (l < -s_max_motor) l = -s_max_motor;
    if (r >  s_max_motor) r =  s_max_motor;
    if (r < -s_max_motor) r = -s_max_motor;

    /* STM32 수신 측이 빅엔디언을 기대하므로 상위 바이트를 먼저 씁니다. */
    uint8_t payload[4];
    payload[0] = (uint8_t)(l >> 8);
    payload[1] = (uint8_t)(l & 0xFF);
    payload[2] = (uint8_t)(r >> 8);
    payload[3] = (uint8_t)(r & 0xFF);

    can_send(s_can_fd, ID_F446_MOTOR, payload, 4, "Nav2 cmd_vel 모터");
    printf("  [Nav2] L=%d  R=%d\n", l, r);
}