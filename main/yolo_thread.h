/*
 * yolo_thread.h
 * 역할: YOLO 스트리머를 별도 스레드로 실행
 * 원래 위치: 통합Main.c yolo_stream_thread() (603~621줄)
 */
#ifndef YOLO_THREAD_H
#define YOLO_THREAD_H

/* YOLO 스레드 시작 (pthread_create + pthread_detach) */
int yolo_thread_start(void);

#endif