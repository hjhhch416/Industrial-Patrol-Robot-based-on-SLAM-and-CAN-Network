/*
 * nav2_bridge.h
 * 역할: Nav2 cmd_vel UDP 수신 → CAN 모터 명령 변환·전송
 * 원래 위치: 통합Main.c recv_cmdvel_and_send() (273~317줄)
 */
#ifndef NAV2_BRIDGE_H
#define NAV2_BRIDGE_H

/*
 * nav2_bridge_init()
 *   사용할 fd와 최대 모터값을 주입합니다.
 *   init_main()에서 한 번 호출합니다.
 */
void nav2_bridge_init(int cmdvel_fd, int can_fd, int max_motor_val);

/*
 * nav2_recv_and_send()
 *   UDP:10000에서 최신 cmd_vel 패킷을 읽어 CAN 0x100으로 전송합니다.
 *   수신 없으면 아무것도 안 합니다.
 *   정상 상태(STATE_NORMAL)일 때 메인 루프에서 호출합니다.
 */
void nav2_recv_and_send(void);

#endif