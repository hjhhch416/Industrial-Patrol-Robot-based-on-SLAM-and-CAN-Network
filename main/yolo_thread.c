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
    printf("📹 [YOLO] 시스템 안정화 대기 중 (3초)...\n");
    sleep(3);
    // CAN/소켓/WebSocket 초기화 완료까지 3초 대기

    system("pkill -9 ffmpeg");           // 이전 ffmpeg 프로세스 종료
    system("pkill -f yolo_streamer.py"); // 이전 YOLO 스크립트 종료
    sleep(1);

    printf("📹 [YOLO] 영상 처리 및 송출 프로세스 시작...\n");
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