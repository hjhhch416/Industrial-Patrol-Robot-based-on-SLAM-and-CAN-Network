/*
 * util.c  —  UDP 유틸리티
 *
 * [통합Main.c에서 이 파일로 옮긴 함수]
 *   create_udp_socket() : 원래 통합Main.c 255~281줄
 *   recv_latest_udp()   : 원래 통합Main.c 284~297줄
 *
 * [통합Main.c에 그대로 둔 함수]
 *   get_ms()      95~99줄   — 통합Main.c 내부에서만 사용
 *   elapsed_sec() 204~210줄 — 통합Main.c 내부에서만 사용
 */

#include "util.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ─────────────────────────────────────────────────
 * create_udp_socket()
 *   원래 통합Main.c 255~281줄. 내용 동일.
 * ───────────────────────────────────────────────── */
int create_udp_socket(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[에러] UDP 소켓 생성 실패");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[에러] UDP 바인드 실패");
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("[*] UDP 소켓 준비 완료 (포트 %d)\n", port);
    return fd;
}

/* ─────────────────────────────────────────────────
 * recv_latest_udp()
 *   원래 통합Main.c 284~297줄. 내용 동일.
 * ───────────────────────────────────────────────── */
int recv_latest_udp(int udp_fd)
{
    char    buf[8];
    int     latest = -1;
    ssize_t n;

    while ((n = recvfrom(udp_fd, buf, sizeof(buf) - 1,
                         0, NULL, NULL)) > 0) {
        buf[n] = '\0';
        if      (buf[0] == '1') latest = 1;
        else if (buf[0] == '0') latest = 0;
    }
    return latest;
}

/* ─────────────────────────────────────────────────
 * get_ms()
 *   단조 시계 기준 현재 시각을 ms로 반환합니다.
 *   alarm.c 분리 후 main.c 밖에서도 필요해져 util로 이동.
 * ───────────────────────────────────────────────── */
long long get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

/* ─────────────────────────────────────────────────
 * elapsed_sec()
 *   start 시점부터 현재까지 경과한 시간을 초(double)로 반환합니다.
 *   alarm.c 분리 후 main.c 밖에서도 필요해져 util로 이동.
 * ───────────────────────────────────────────────── */
double elapsed_sec(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec  - start.tv_sec)
         + (now.tv_nsec - start.tv_nsec) / 1e9;
}