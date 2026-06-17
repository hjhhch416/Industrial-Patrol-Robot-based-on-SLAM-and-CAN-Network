# 🔧 Raspberry Pi 5 Rover Integration Controller

---

## 📌 역할 및 개요

Raspberry Pi 5는 로버의 상위 통합 중계 노드로 동작합니다.

WebSocket으로 Node.js 서버의 명령을 수신하고 CAN USB 어댑터를 통해 **STM32F446 / STM32F429에 명령을 송신**합니다. STM32로부터 수신한 센서 상태(속도, 거리, 조도 등)는 WebSocket으로 서버에 송신해 웹 대시보드에 표시합니다.

자율주행 모드에서는 Nav2(ROS2)의 cmd_vel 명령을 UDP로 수신해 CAN 모터 명령으로 변환하며, YOLO 위험 감지 신호에 따라 경보 또는 긴급 정지 명령을 STM32에 송신합니다.

| 항목 | 내용 |
|---|---|
| 플랫폼 | Raspberry Pi 5 |
| 언어 | C (gcc) |
| 빌드 | Makefile |
| 통신 | CAN USB, WebSocket, UDP |
| 주요 기능 | 명령 중계, 자율주행 cmd_vel 변환, 경보 처리, 센서 데이터 송신 |

---

## ✨ 주요 기능

- WebSocket 기반 원격 명령 수신 (모터, 서보, 모드 전환, 조명, 경보)
- CAN USB 어댑터를 통해 STM32F446 / STM32F429로 명령 송신
- STM32 센서 데이터 수신 후 JSON으로 변환해 서버로 송신
- Nav2 cmd_vel UDP 수신 → CAN 모터 명령 변환 (자율주행 모드)
- YOLO 위험 감지 UDP 수신 → 부저 / LED 경보 처리
- 수동 모드 / 자율주행 모드별 경보 동작 분리
- YOLO 스트리머를 별도 스레드로 실행

---

## 🧩 시스템 구조

```text
Node.js Server / Nav2 / YOLO
            |
            | WebSocket / UDP
            v
+-----------------------------+
|      Raspberry Pi 5         |
|                             |
|  WebSocket Client           |
|  UDP Receiver (Nav2/YOLO)   |
|  CAN TX/RX                  |
|  Alarm State Machine        |
|  Sensor Data Relay          |
+-----------------------------+
            |
            | CAN (USB-CAN-A)
            v
   STM32F446 / STM32F429
```

---

## 🔀 동작 흐름

통합 컨트롤러(`main.c`)는 100 ms 주기로 5개 태스크를 순차 실행하며, 들어온 신호를 알맞은 장치로 중계합니다. 동작(무엇을 하는가)은 크게 5가지 시나리오로 나뉩니다.

<p align="center">
  <img src="https://github.com/user-attachments/assets/726b7f13-46cc-4241-8936-c25fc86be8e7" width="760" alt="동작 흐름 구조도" />
</p>

- **① 원격 제어** — 웹 대시보드 서버(Node.js)의 조작 명령을 `ws_client`가 받아 제어 보드(F446·F429)로 전달합니다.
- **② 센서 보고** — 제어 보드의 센서 상태를 `ws_client`가 모아 JSON으로 서버에 올립니다.
- **③ 수동 경보** — 수동 모드에서 `yolo_streamer`의 위험 신호를 `alarm`이 받아 부저·경고등(F429)만 켜고, 주행은 사용자가 유지합니다.
- **④ 자율 경보** — 자율 모드에서 위험 신호 수신 시 `alarm`이 모터(F446)를 즉시 정지하고 부저·경고등(F429)을 켭니다.
- **⑤ 자율 주행** — Nav2의 cmd_vel 주행 명령을 `nav2_bridge`가 CAN 모터 명령(F446)으로 변환해 전달합니다.

### 🧱 메인 루프 태스크

위 동작은 실제로(어떻게 실행되는가) 100 ms 루프 안에서 5개 태스크 함수로 순차 실행됩니다. 시나리오(①~⑤)와 태스크(1~5)는 1:1이 아닙니다 — 태스크 2·3이 함께 ②를, 태스크 5가 ④·⑤를 처리합니다.

```text
태스크 1 : ws_client_service()        - WebSocket 이벤트 처리        → ①
태스크 2 : ws_client_request_write()  - 대시보드 갱신 예약           ┐
태스크 3 : can_rx_poll() + set_status - CAN 수신 + 상태 갱신          ┘→ ②
태스크 4 : alarm_tick_manual()        - 수동 모드 경보               → ③
태스크 5 : alarm_tick_auto()          - 자율주행 모드 경보 + Nav2    → ④·⑤
```

---

## 📁 폴더 구조

```text
main
├── main.c              # 메인 루프, 5개 태스크
├── ws_client.c/h       # WebSocket 클라이언트
├── alarm.c/h           # 경보 처리
├── nav2_bridge.c/h     # Nav2 cmd_vel 변환
├── robot_can.c/h       # CAN 프로토콜 레이어
├── canusb_lib.c/h      # CAN USB 시리얼 드라이버
├── util.c/h            # UDP 소켓, 타이밍 유틸리티
├── yolo_thread.c/h     # YOLO 스트리머 스레드
├── Makefile
└── README.md
```

---

## 🗂️ 주요 파일

| 파일 | 역할 |
|---|---|
| `main.c` | 초기화 및 메인 루프 |
| `ws_client.c/h` | Node.js 서버 연결, 명령 수신, 센서 JSON 송신 |
| `alarm.c/h` | YOLO 신호 기반 경보 상태 관리 |
| `nav2_bridge.c/h` | Nav2 UDP 패킷 파싱 및 CAN 변환 |
| `robot_can.c/h` | CAN ID 정의, 송수신 함수 |
| `canusb_lib.c/h` | 시리얼 포트 초기화, 프레임 송수신 |
| `util.c/h` | UDP 소켓 생성, 타이밍 유틸 |
| `yolo_thread.c/h` | YOLO Python 스크립트 실행 스레드 |

---

## 🔌 통신 포트 및 인터페이스

| 인터페이스 | 설정 | 설명 |
|---|---|---|
| CAN USB | `/dev/ttyUSB0` | USB-CAN-A 어댑터 (시리얼 2 Mbps / CAN 버스 500 kbps) |
| WebSocket | `192.168.0.48:5000` | Node.js 서버 연결 |
| UDP YOLO | `:9999` | YOLO 위험 감지 신호 수신 |
| UDP Nav2 | `:10000` | Nav2 cmd_vel 수신 |

---

## 📡 CAN 프로토콜

### 모터 명령 송신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x100` |
| 방향 | Raspberry Pi → STM32F446 |
| 데이터 | 좌/우 모터 속도 |

```text
Data[0] : Left Speed High
Data[1] : Left Speed Low
Data[2] : Right Speed High
Data[3] : Right Speed Low
```

속도 데이터는 signed 16-bit 정수입니다.

```text
Left Speed  = int16_t((Data[0] << 8) | Data[1])
Right Speed = int16_t((Data[2] << 8) | Data[3])
```

### 서보 명령 송신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x300` |
| 방향 | Raspberry Pi → STM32F446 |
| 데이터 | 서보 목표 각도 |

```text
Data[0] : Servo Angle
```

서보 각도 범위는 `0 ~ 180도`입니다.

### 알림 명령 송신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x101` / `0x102` / `0x103` / `0x104` |
| 방향 | Raspberry Pi → STM32F429 |
| 데이터 | ON/OFF 또는 모드 |

```text
0x101 : RGB 조명 ON/OFF
0x102 : 부저 ON/OFF
0x103 : 경고 LED ON/OFF
0x104 : 모드 전환 (0=AUTO, 1=MANUAL)
```

### F446 상태 수신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x200` |
| 방향 | STM32F446 → Raspberry Pi |
| 수신 주기 | 1000 ms |

```text
Data[0] : AEB Flag
Data[1] : Servo Angle
Data[2] : Distance High
Data[3] : Distance Low
Data[4] : Left Speed High
Data[5] : Left Speed Low
Data[6] : Right Speed High
Data[7] : Right Speed Low
```

### F429 상태 수신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x105` |
| 방향 | STM32F429 → Raspberry Pi |

```text
Data[0] : Lux High
Data[1] : Lux Low
Data[2] : RGB LED 상태
Data[3] : 경고 LED 상태
Data[4] : 부저 상태
```

---

## 🔔 UDP 인터페이스

### YOLO 위험 감지 수신

| 항목 | 내용 |
|---|---|
| 포트 | `:9999` |
| 방향 | YOLO → Raspberry Pi |
| 데이터 | `"1"` / `"0"` |

```text
"1" : 위험 감지
"0" : 위험 해제
```

### Nav2 cmd_vel 수신

| 항목 | 내용 |
|---|---|
| 포트 | `:10000` |
| 방향 | Nav2 → Raspberry Pi |
| 데이터 | 좌/우 모터 속도 (16진수 ASCII 8자리) |

```text
"LLLLRRRR"
앞 4자리 = Left Speed  (int16, 빅엔디언 16진수)
뒤 4자리 = Right Speed (int16, 빅엔디언 16진수)
```

예시: `"01F4FE0C"` → Left = +500, Right = -500

---

## 🛑 경보 처리 로직

YOLO 위험 감지 신호 수신 시 모드에 따라 다르게 동작합니다. 수동 모드에서는 부저·LED만 켜고 모터는 사용자 제어를 유지하며, 자율주행 모드에서는 모터를 즉시 정지합니다.

| 항목 | 값 |
|---|---:|
| 경보 지속 시간 | 3 초 |
| 쿨다운 | 2 초 |
| 모터 정지 재송신 주기 | 1000 ms |

수동 모드는 모터를 그대로 두고 부저·LED만 제어합니다.

```c
if (received == 1 && !s_manual_active && !in_cooldown) {
    can_send(s_can_fd, ID_F429_BUZZER, &ON_VAL, 1, "부저 ON");
    can_send(s_can_fd, ID_F429_LED,    &ON_VAL, 1, "LED ON");
}
```

자율주행 모드에서는 모터를 정지시키고 경보 중에도 1초마다 정지 명령을 재송신합니다.

```c
if (received == 1 && s_state == STATE_NORMAL && !in_cooldown) {
    can_send(s_can_fd, ID_F446_MOTOR, stop, 4, "자율주행 모터 정지");
    can_send(s_can_fd, ID_F429_BUZZER, &ON_VAL, 1, "부저 ON");
    can_send(s_can_fd, ID_F429_LED,    &ON_VAL, 1, "LED ON");
}
```

---

## ⚙️ 제어 주기

| 항목 | 주기 |
|---|---:|
| 메인 루프 | 100 ms |
| CAN 수신 polling | 100 ms |
| 대시보드 갱신 (WebSocket) | 매 루프 |
| 경보 모터 정지 재송신 | 1000 ms |
| 경보 지속 시간 | 3000 ms |
| 경보 쿨다운 | 2000 ms |

---

## 🏗️ 빌드 및 실행

### 설정 체크리스트

- CAN 어댑터는 `/dev/ttyUSB0`에 연결
- CAN bitrate 500 kbps (STM32와 동일하게 설정)
- Node.js 서버 IP/포트 일치 확인
- ROS2 Nav2 `cmd_vel_to_can_udp.py` 노드 실행 확인
- YOLO 스크립트 경로 환경에 맞게 수정

### 빌드

다음 의존 라이브러리를 설치합니다.

```bash
sudo apt install libwebsockets-dev libjansson-dev
```

Makefile로 빌드한 뒤 실행합니다. CAN 어댑터 접근을 위해 `sudo`가 필요합니다.

```bash
make
sudo ./robot_main
```

빌드 결과물을 삭제하려면 다음 명령을 사용합니다.

```bash
make clean
```

---

## ⚠️ 주의사항

- CAN 어댑터는 `/dev/ttyUSB0`에 연결합니다. 다른 USB 시리얼 장치가 먼저 연결되어 있으면 `ttyUSB1` 등으로 할당될 수 있습니다.
- Node.js 서버가 실행 중이어야 WebSocket이 연결됩니다.
- YOLO 스크립트 경로(`/home/lee/project/test4/yolo_streamer.py`)는 실행 환경에 맞게 수정해야 합니다.
- CAN bitrate는 STM32와 반드시 동일하게 500 kbps로 설정되어 있어야 합니다.
- 실행 시 `sudo` 권한이 필요합니다 (CAN 어댑터 접근, YOLO 스크립트 실행).
- Nav2 자율주행은 `cmd_vel_to_can_udp.py` 브릿지 노드가 함께 실행되어야 동작합니다.

---

## 📝 프로젝트 요약

이 프로그램은 Raspberry Pi 5에서 실행되며 상위 서버와 하위 STM32 사이의 통합 중계를 담당합니다.

WebSocket으로 Node.js 서버의 명령을 수신해 CAN 버스를 통해 STM32F446(모터·서보)과 STM32F429(부저·LED)로 명령을 송신합니다. STM32로부터 수신한 센서 상태는 WebSocket으로 다시 서버에 송신해 웹 대시보드에 표시합니다.

자율주행 모드에서는 Nav2(ROS2)의 cmd_vel 명령을 UDP로 수신해 CAN 모터 명령으로 변환하여 STM32F446에 송신합니다. YOLO 위험 감지 신호를 UDP로 수신한 경우 모드에 따라 경보 또는 긴급 정지 명령을 STM32에 송신합니다.
