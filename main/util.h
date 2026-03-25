/*
 * util.h  —  공통 유틸리티
 *
 * [포함 함수]
 *   - create_udp_socket() : UDP 소켓 생성
 *   - recv_latest_udp()   : UDP 최신 패킷 수신
 *   - get_ms()            : 현재 시각 ms 반환
 *   - elapsed_sec()       : 경과 시간 초 반환
 *
 * [변경 이력]
 *   alarm.c 분리 후 get_ms()/elapsed_sec()이
 *   main.c 외부에서도 필요해져 util로 이동
 */

#ifndef UTIL_H
#define UTIL_H

#include <time.h>  // struct timespec

/*
 * create_udp_socket()
 *   지정 포트에 바인딩된 논블로킹 UDP 수신 소켓을 생성합니다.
 *   반환값: fd (성공) / -1 (실패)
 */
int create_udp_socket(int port);

/*
 * recv_latest_udp()
 *   UDP 수신 버퍼를 전부 비우고 마지막 값만 반환합니다.
 *   반환값: 1 (위험), 0 (정상), -1 (이번 루프에 수신 없음)
 */
int recv_latest_udp(int udp_fd);

/*
 * get_ms()
 *   단조 시계(CLOCK_MONOTONIC) 기준 현재 시각을 ms로 반환합니다.
 *   쿨다운/타이밍 체크에 사용합니다.
 */
long long get_ms(void);

/*
 * elapsed_sec()
 *   start 시점부터 현재까지 경과한 시간을 초(double)로 반환합니다.
 *   경보 지속 시간 체크에 사용합니다.
 */
double elapsed_sec(struct timespec start);

#endif /* UTIL_H */