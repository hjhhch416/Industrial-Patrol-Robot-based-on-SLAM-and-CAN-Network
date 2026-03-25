# ============================================================
# Makefile  —  자율주행 로봇 통합 프로그램
# ============================================================

CC      = gcc
CFLAGS  = -Wall -O2 -pthread          # -pthread: pthread_create 등 스레드 함수 사용
LDFLAGS = -lwebsockets -ljansson      # libwebsockets, jansson 링크

TARGET  = robot_main

# ── 소스 파일 목록 ────────────────────────────────────────
# 파일이 추가될 때 여기에만 추가하면 됩니다
SRCS =  main.c            \
        canusb_lib.c      \
        robot_can.c       \
        util.c            \
        nav2_bridge.c     \
        alarm.c           \
        ws_client.c       \
        yolo_thread.c

# .c → .o 오브젝트 파일 목록 자동 생성
OBJS = $(SRCS:.c=.o)

# ── 빌드 규칙 ─────────────────────────────────────────────

# 기본 타겟: 전체 빌드
all: $(TARGET)

# 링크: 오브젝트 파일들을 하나의 실행 파일로 묶기
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 컴파일: 각 .c → .o
# $<  = 현재 규칙의 첫 번째 의존 파일 (.c)
# $@  = 현재 규칙의 타겟 파일 (.o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── 의존성 (헤더 변경 시 관련 .c 재컴파일) ───────────────
main.o:       main.c        canusb_lib.h robot_can.h util.h \
                            nav2_bridge.h alarm.h ws_client.h yolo_thread.h
canusb_lib.o: canusb_lib.c canusb_lib.h
robot_can.o:  robot_can.c  robot_can.h  canusb_lib.h
util.o:       util.c       util.h
nav2_bridge.o:nav2_bridge.c nav2_bridge.h robot_can.h
alarm.o:      alarm.c      alarm.h      robot_can.h util.h
ws_client.o:  ws_client.c  ws_client.h  robot_can.h alarm.h
yolo_thread.o:yolo_thread.c yolo_thread.h

# ── 청소 ─────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(TARGET)

# ── 빌드 + 즉시 실행 ─────────────────────────────────────
run: all
	./$(TARGET)

.PHONY: all clean run