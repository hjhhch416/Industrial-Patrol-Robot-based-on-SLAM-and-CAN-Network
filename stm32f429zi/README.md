## 🛠️기능 제어 노드 개발 (STM32F429ZI)

로봇의 **주변 환경 센싱(조도) 및 시청각 경고 시스템(LED, Buzzer) 제어**, 그리고 **CAN 기반 분산 제어 통신**을 담당하는 하위 제어 노드를 구현했습니다.

- STM32F429ZI 기반 Function Control Node 개발
- CAN Bus 기반 제어 메시지 송수신 구조 구현
- Raspberry Pi 5와 STM32 제어 보드 간 시스템 연동
- LED, Buzzer, RGB LED, Photo Sensor 제어
- GPIO, ADC, PWM, CAN Peripheral 설정 및 제어
- Servo Motor PWM 출력 불안정 문제 분석
- 전원 공급 및 GND 연결 문제 분석을 통한 하드웨어 안정화

## ⚙️ 사용 기술

### Embedded

- STM32F429ZI
- STM32F446
- STM32CubeIDE
- C
- HAL Driver
- GPIO
- ADC
- PWM
- CAN Communication

### Robot System

- Raspberry Pi 5
- ROS2
- LiDAR
- RealSense D435
- YOLOv5
- OpenCV

### Hardware

- Servo Motor
- Buzzer
- RGB LED
- Photo Sensor
- Ultrasonic Sensor
- CAN Transceiver

## 🖥️ 시스템 구조
<img width="823" height="587" alt="image (2)" src="https://github.com/user-attachments/assets/1e4a15e7-69c7-4938-b885-606c463b96da" />


```text
Raspberry Pi 5
 ├── ROS2
 ├── SLAM
 ├── YOLO Object Detection
 └── CAN Communication
        ↓
STM32F429ZI
 ├── LED Control
 ├── Buzzer Control
 ├── RGB LED Control
 ├── Photo Sensor ADC
 └── Servo Motor PWM
        ↓
STM32F446
 └── Motor Control
```
### 🔍 핀맵 (Pin Mapping)
<img width="1920" height="1080" alt="작업장 순찰 로봇 pptx" src="https://github.com/user-attachments/assets/f1b3982c-c19e-4f1f-9ed7-43f5bfd354db" />

| 분류 | 핀 번호 (Pin) | 기능 (Function) | 연결 대상 (Target) | 비고 (Description) |
| :---: | :---: | :---: | :---: | :--- |
| **통신** | PB8 | CAN1_RX | STM32F446 (PA11) | 보드 간 통신 인터페이스 (Baudrate: 500kbps) |
| **통신** | PB9 | CAN1_TX | STM32F446 (PA12) | 보드 간 통신 인터페이스 (Baudrate: 500kbps) |
| **센서** | PA3 | ADC1_IN3 | 포토센서 (CdS) | 연속 샘플링 모드, 데이터 정밀도를 위해 56 사이클 이상의 샘플링 타임 확보 |
| **출력** | PB4 | TIM3_CH1 (PWM) | 경고 부저 (Buzzer) | PSC: 179, ARR: 999 설정 / 인간 청각 민감 대역 및 공진 주파수(3~4kHz) 제어, 50% 듀티비 |
| **출력** | PE9 | TIM1_CH1 (PWM) | RGB LED (Red) | PSC: 179, ARR: 999 (1kHz 주파수), 왜곡(Glitch) 방지를 위한 Auto-reload preload 활성화 |
| **출력** | PE11 | TIM1_CH2 (PWM) | RGB LED (Green) | PSC: 179, ARR: 999 (1kHz 주파수), 왜곡(Glitch) 방지를 위한 Auto-reload preload 활성화 |
| **출력** | PE13 | TIM1_CH3 (PWM) | RGB LED (Blue) | PSC: 179, ARR: 999 (1kHz 주파수), 왜곡(Glitch) 방지를 위한 Auto-reload preload 활성화 |
| **출력** | PF15 | GPIO_Output | 단색 LED (단단) | 일반 고휘도 LED 기준 상태 표시용 디지털 출력 (No pull-up/pull-down) |
| **인터페이스**| - | RMII / USB / UART | 이더넷 및 주변 장치 노드 | LAN8742A-CZ-TR 인터페이스 연동 및 관제 센터향 저지연 인프라 확장성 확보 |

### 📌 주요 개발 내용

#### 1. CAN 통신 기반 실시간 분산 제어 네트워크 구현
* **비동기 인터럽트 기반 명령 수신:** 상위 제어기(Raspberry Pi 5)로부터 하달되는 비동기 제어 명령을 실시간으로 처리하기 위해 `CAN RX FIFO0` 메시지 보류 인터럽트를 활용했습니다.
* **프로토콜 ID 기반 분기(Callback) 구조:** 수신된 메시지 ID(`0x101`~`0x104`)에 따라 운영 모드(Auto/Manual) 전환 및 조명/경고 장치의 상태 변수를 즉각 갱신하도록 콜백 로직을 설계했습니다.
* **주기적 상태 보고 (Telemetry):** 조도 센서 값(12비트), RGB LED, 경고 LED, 부저의 현재 상태를 5바이트 데이터 팩으로 패킹하여 `0x105` ID로 상위 관제 시스템에 200ms 주기로 송신합니다.

#### 2. ADC 연동 지능형 조명 제어 시스템 (Auto Mode)
* **환경 데이터 수집:** `ADC1_IN3` 채널(PA3)을 통해 조도 센서(Photoresistor)의 아날로그 값을 실시간 폴링(Polling) 방식으로 수집합니다.
* **자동화 로직 구현:** 수집된 조도 값이 임계치(Dark Threshold = 2000) 미만일 경우 어두운 작업 환경으로 판단하여 고휘도 화이트 조명(RGB LED)을 자동 점등시키고, 밝아지면 소등되도록 상태 머신(FSM)을 구성했습니다.
* **수동 제어 예외 처리:** 수동 제어(Manual) 모드 진입 시에는 상위 제어기의 CAN 명령에 의해서만 조명이 동기화되도록 예외 처리를 수행했습니다.

#### 3. 타겟 추적 기반 시청각 경고 장치 제어
Vision AI(YOLOv5)에서 안전장비 미착용자를 검출했을 때, 작업자에게 즉각적인 경고를 주기 위한 액추에이터 제어를 구현했습니다.
* **하드웨어 타이머 분할 제어:** * `TIM1 (CH1, CH2, CH3)`: PWM mode 1을 적용하여 RGB LED의 밝기를 하드웨어적으로 정밀 제어합니다.
  * `TIM3 (CH1)`: 하드웨어 경보음을 위해 Duty Cycle 50%(Pulse 500)를 인가하여 명확한 부저음을 송출합니다.
  * `GPIOF (Pin 15)`: 적색 산업용 경고 LED를 점등합니다.

#### 4. 💡 트러블슈팅 및 아키텍처 개선
* **문제 상황:** 초기 단일 보드 구성 시, 전류 소모가 큰 모터 구동과 CAN 트랜시버가 전원을 공유하면서 전압 강하가 발생했고, GND 레벨이 맞지 않아 CAN 통신 신호가 불안정해지는 이슈가 발생했습니다.
* **해결 방안:** 로봇의 안정성을 높이기 위해 모터 구동 노드(STM32F446RE)와 기능 제어 노드(STM32F429ZI)를 분리하는 **분산 제어 아키텍처**로 개편했습니다. 이후 보드 간 GND 레벨을 동기화시켜 안정적인 전원 공급 및 통신 환경을 성공적으로 확보했습니다.

