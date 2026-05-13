/*
 * yolo_thread.c
 * 원래 위치: 통합Main.c yolo_stream_thread() (603~621줄)
 */
#include "yolo_thread.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

static void *yolo_stream_thread(void *arg)
{
    printf("[YOLO] 시스템 안정화 대기 중 (3초)...\n");
    /* CAN 어댑터·UDP 소켓·WebSocket이 모두 초기화될 때까지 기다립니다.
     * YOLO가 너무 일찍 뜨면 UDP 전송 대상 소켓이 아직 없어 패킷이 유실됩니다. */
    sleep(3);

    /* 이전 실행에서 남은 프로세스를 정리합니다.
     * 비정상 종료 후 재시작 시 포트 충돌·GPU 메모리 점유를 막기 위함입니다. */
    system("pkill -9 ffmpeg");
    system("pkill -f yolo_streamer.py");
    sleep(1);

    printf("[YOLO] 영상 처리 및 송출 프로세스 시작...\n");
    /* system()으로 Python 스크립트를 자식 프로세스로 실행합니다.
     * 스크립트가 종료되면 이 스레드도 함께 종료됩니다(정상 동작). */
    system("sudo /home/lee/yolo_env/bin/python3 /home/lee/project/test4/yolo_streamer.py");

    return NULL;
}

int yolo_thread_start(void)
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, yolo_stream_thread, NULL) != 0) {
        perror("[에러] YOLO 스레드 생성 실패");
        return -1;
    }
    pthread_detach(tid); // 메인 스레드와 독립 실행
    printf("[*] YOLO 스트리머 스레드 시작\n");
    return 0;
}