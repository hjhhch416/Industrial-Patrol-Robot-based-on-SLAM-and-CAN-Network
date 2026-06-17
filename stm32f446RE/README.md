# 🔧 STM32F446 Rover Control Firmware

---

## 📌 역할 및 개요
<p align="center">
  <img src="https://github.com/user-attachments/assets/bb073d51-30e2-41bc-a275-776d4f3fd4b6"
       width="180"
       alt="stm32f446 board" />
</p>

STM32F446 보드는 로버의 하위 제어 노드로 동작합니다.

Raspberry Pi(상위 제어기)와 CAN 통신으로 연결되어 로버의 주행(DC 모터)과 카메라 방향(서보 모터)을 제어하며,

초음파 센서로 후진 시 장애물을 감지해 안전 정지(AEB)를 수행하는 펌웨어입니다.


| 항목 | 내용 |
|---|---|
| MCU | STM32F446 |
| 언어 | C, STM32 HAL |
| 통신 | CAN |
| 주요 기능 | 모터 제어, 서보 제어, 초음파 거리 측정, 안전 정지, 상태 송신 |

---

## ✨ 구동부 하드웨어 회로 이미지
<p align="center">
  <img src="https://github.com/user-attachments/assets/cade5bed-9518-4538-a788-6cb650438f52"
       width="360"
       alt="rover5 motor driver" />
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/1e37e695-57f1-4b2a-89c4-bfb9659fb245"
       width="720"
       alt="stm32f446 motor wiring" />
</p>

---

## 🗂️ 주요 모듈 및 파일
제어 흐름, 통신, 센서 처리 등 각 기능이 모듈화되어 있습니다.
| 모듈 | 파일명 | 핵심 역할 |
|---|---|---|
| **메인 제어** | `ap.c / h` | 10ms 메인 제어 루프 및 각 모듈 초기화/콜백 연결 |
| **통신** | `can_comm.c / h` | CAN 명령어 RX(수신) 및 상태 데이터 TX(송신) 처리 |
| **모터 제어** | `motor.c / h` | 좌/우 DC 모터 PWM/방향 제어 및 부드러운 가감속(Ramp) 제어 |
| **서보 제어** | `servomotor.c / h` | 서보 모터 각도(0\~180도)를 PWM 펄스(500\~2500us)로 변환 적용 |
| **초음파 센서** | `ultrasonic.c / h` | DWT 딜레이와 TIM2 Input Capture를 이용한 에코 시간 측정 |
| **안전 정지** | `safe_distance.c / h` | 스파이크 튀는 현상 제거용 필터 및 히스테리시스 기반 충돌 방지 로직 |

---

## ⏱️ 핀맵 및 타이머
<img width="598" height="555" alt="stm32446_pinmap" src="https://github.com/user-attachments/assets/5bcd8f33-422b-4123-b73d-216a81734857" />

| 기능 | 타이머/핀 설정 | 설명 |
|---|---|---|
| Left / Right Motor | TIM3_CH1, CH2 / DIR1, 2 | 좌우 모터 속도(PWM) 및 방향 제어 |
| Ultrasonic 센서 | TIM2_CH1 / TRIG 핀 | 에코 시간 캡처(Input) 및 10us 트리거 출력 |
| Servo Motor | TIM4_CH1 | 카메라 각도 조절용 PWM 출력 |
| CAN 통신 | CAN1 (RX FIFO0) | 상위 제어기 명령어 수신 및 로버 상태 송신 |

---

## 📡 CAN 프로토콜

### 모터 명령 수신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x100` |
| 방향 | Raspberry Pi → STM32 |
| 데이터 | 좌/우 모터 속도 |

```text
Data[0] : Left Speed High
Data[1] : Left Speed Low
Data[2] : Right Speed High
Data[3] : Right Speed Low
```

속도 데이터는 signed 16-bit 정수로 해석합니다.

```text
Left Speed  = int16_t((Data[0] << 8) | Data[1])
Right Speed = int16_t((Data[2] << 8) | Data[3])
```

### 서보 명령 수신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x300` |
| 방향 | Raspberry Pi → STM32 |
| 데이터 | 서보 목표 각도 |

```text
Data[0] : Servo Angle
```

서보 각도 범위는 `0 ~ 180도`입니다.


### 상태 송신

| 항목 | 내용 |
|---|---|
| CAN ID | `0x200` |
| 방향 | STM32 → Raspberry Pi |
| 송신 주기 | 1000 ms |

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

| AEB Flag | 의미 |
|---|---|
| `0` | 정상 |
| `1` | 안전 정지 중 |

---

## 🛑 제어 주기 및 안전 정지(AEB) 로직
* **제어 주기:** 메인 로프 `10ms`, 초음파 트리거 `60ms`, 상태 CAN 송신 `1000ms` 주기로 동작합니다.
* **안전 정지:** 로버 **후진 명령 시**에만 개입하며, 초음파 거리 측정값이 **15cm 이하로 2회 연속** 잡히면 모터 출력을 강제로 0으로 차단합니다.
* **정지 해제:** 측정 거리가 **20cm 이상 2회 연속** 잡힐 경우 다시 정상 주행이 가능하도록 복귀됩니다. (센서 타임아웃 200ms 초과 시에도 강제 정지 처리)
  <img width="694" height="194" alt="ultrasonic_logic" src="https://github.com/user-attachments/assets/43046851-b81f-4fc1-adf6-41812cf4f5ac" />

---

## ⚠️ 주요 주의사항
* STM32와 Raspberry Pi(또는 상위 제어기)의 CAN Bitrate는 반드시 동일하게 맞춰야 합니다.
* 정확한 초음파 센서 거리 측정을 위해 TIM2의 프리스케일러를 조절해 1 Tick = 1us(1MHz) 단위로 맞추는 것을 권장합니다.
* 좌/우 모터의 실제 구동 방향이 배선에 의해 반대로 동작할 경우, `motor_init` 함수의 `dir_invert` 옵션을 활성화(1)하여 소프트웨어적으로 쉽게 보정할 수 있습니다.

---
