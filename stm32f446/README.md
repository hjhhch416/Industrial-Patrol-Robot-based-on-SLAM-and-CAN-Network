# 🔧 STM32F446 Rover Control Firmware

---

## 📌 역할 및 개요
<p align="center">
  <img src="https://github.com/user-attachments/assets/bb073d51-30e2-41bc-a275-776d4f3fd4b6"
       width="180"
       alt="stm32f446 board" />
</p>

STM32F446 보드는 로버의 하위 제어 노드로 동작합니다.

Raspberry Pi 또는 상위 제어기에서 CAN 통신으로 **좌/우 모터 속도 명령**과 **서보 카메라 각도 명령**을 수신하고, STM32는 DC 모터, 서보 모터, 초음파 센서를 제어합니다.

후진 중 초음파 센서로 장애물이 감지되면 모터 출력을 0으로 제한하여 안전 정지를 수행합니다.

| 항목 | 내용 |
|---|---|
| MCU | STM32F446 |
| 언어 | C, STM32 HAL |
| 통신 | CAN |
| 주요 기능 | 모터 제어, 서보 제어, 초음파 거리 측정, 안전 정지, 상태 송신 |

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

## ✨ 주요 기능

- CAN 기반 좌/우 모터 속도 명령 수신
- CAN 기반 서보 카메라 각도 명령 수신
- DC 모터 PWM/DIR 제어
- 서보 모터 PWM 제어
- 초음파 센서 거리 측정
- 후진 중 장애물 감지 시 안전 정지
- CAN 상태 데이터 송신

---

## 🧩 시스템 구조

```text
Raspberry Pi / Server
        |
        | CAN
        v
+---------------------+
|      STM32F446      |
|                     |
|  CAN RX/TX          |
|  Motor Control      |
|  Servo Control      |
|  Ultrasonic Sensor  |
|  Safe Stop Logic    |
+---------------------+
        |
        v
DC Motor / Servo / Ultrasonic Sensor
```

---

## 📁 폴더 구조

```text
stm32f446
├── App/ap              # 사용자 작성 제어 모듈
├── Core                # STM32CubeMX 생성 코드
├── Drivers             # HAL/CMSIS 드라이버
├── cmake               # CMake 관련 설정
├── CMakeLists.txt
├── CMakePresets.json
├── STM32F446XX_FLASH.ld
├── startup_stm32f446xx.s
├── stm32f446.ioc
└── README.md
```

---

## 🗂️ 주요 파일

| 파일 | 역할 |
|---|---|
| `ap.c` | 전체 제어 흐름 관리 |
| `ap.h` | 핀맵 및 주요 함수 선언 |
| `motor.c/h` | DC 모터 PWM 및 방향 제어 |
| `servomotor.c/h` | 서보 모터 각도 제어 |
| `ultrasonic.c/h` | 초음파 센서 거리 측정 |
| `safe_distance.c/h` | 거리 기반 안전 정지 판단 |
| `can_comm.c/h` | CAN 송수신 처리 |

---

## ⏱️ 핀맵 및 타이머
<img width="598" height="555" alt="stm32446_pinmap" src="https://github.com/user-attachments/assets/5bcd8f33-422b-4123-b73d-216a81734857" />

| 기능 | 설정 | 설명 |
|---|---|---|
| Left Motor PWM | TIM3_CH1 | 왼쪽 모터 속도 제어 |
| Right Motor PWM | TIM3_CH2 | 오른쪽 모터 속도 제어 |
| Left Motor DIR | `DIR1_rover` | 왼쪽 모터 방향 제어 |
| Right Motor DIR | `DIR2_rover` | 오른쪽 모터 방향 제어 |
| Ultrasonic ECHO | TIM2_CH1 | 초음파 ECHO Input Capture |
| Ultrasonic TRIG | `TRIG_ultrasonic` | 초음파 Trigger 출력 |
| Servo PWM | TIM4_CH1 | 카메라 서보 모터 제어 |
| CAN RX/TX | CAN1 | 상위 제어기와 CAN 통신 |

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

## 🛑 안전 정지 로직

초음파 센서로 측정한 거리를 기준으로 후진 중 장애물이 가까워지면 모터 출력을 0으로 제한합니다.
<img width="694" height="194" alt="ultrasonic_logic" src="https://github.com/user-attachments/assets/43046851-b81f-4fc1-adf6-41812cf4f5ac" />

| 항목 | 값 |
|---|---:|
| 정지 거리 | 15 cm |
| 정지 해제 거리 | 20 cm |
| 정지 조건 | 2회 연속 |
| 해제 조건 | 2회 연속 |
| 센서 타임아웃 | 200 ms |

안전 정지는 현재 **후진 명령에서만 적용**됩니다.

```c
if (l_tgt_raw < 0 && r_tgt_raw < 0) {
  safe_distance_apply_cmd(&sd,
                          l_tgt_raw,
                          r_tgt_raw,
                          now_ms,
                          &l_tgt_safe,
                          &r_tgt_safe);
}
```

안전 정지 상태에서는 최종 모터 명령이 0으로 제한됩니다.

```c
if (sd->state == SAFE_ESTOP) {
  *left_out = 0;
  *right_out = 0;
}
```

---

## ⚙️ 제어 주기

| 항목 | 주기 |
|---|---:|
| 메인 제어 루프 | 10 ms |
| 초음파 Trigger | 60 ms |
| CAN 상태 송신 | 1000 ms |
| 초음파 타임아웃 | 50 ms |
| 안전 거리 타임아웃 | 200 ms |

---

## 🧱 CubeMX 설정

```text
TIM2: Input Capture, CH1, 초음파 ECHO 측정
TIM3: PWM, CH1/CH2, 좌/우 모터 제어
TIM4: PWM, CH1, 서보 모터 제어
CAN1: Standard ID, RX FIFO0 Interrupt 사용
GPIO: DIR 핀 Output, TRIG 핀 Output
```

### 설정 체크리스트

- TIM2 Input Capture interrupt 활성화
- TIM3 PWM CH1/CH2 활성화
- TIM4 PWM CH1 활성화
- CAN1 RX FIFO0 interrupt 활성화
- STM32와 Raspberry Pi의 CAN bitrate 동일하게 설정
- TIM2는 `1 tick = 1us` 설정 권장
- 서보 PWM은 `50Hz` 설정 권장

---

## 🔔 인터럽트 연결

### 초음파 Input Capture

`main.c`의 `HAL_TIM_IC_CaptureCallback()`에서 초음파 캡처 함수를 호출합니다.

```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  apUltrasonic_OnCapture(htim);
}
```

### CAN RX

`main.c`의 `HAL_CAN_RxFifo0MsgPendingCallback()`에서 CAN 수신 처리 함수를 호출합니다.

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  can_comm_rx_handler(hcan);
}
```

---

## 🏗️ 빌드 및 플래시

STM32CubeIDE에서 프로젝트를 열고 빌드한 뒤 보드에 업로드합니다.

```text
STM32CubeIDE → Import Project → Build → Flash
```

CMake를 사용하는 경우 다음 명령으로 빌드할 수 있습니다.

```bash
cmake --list-presets
cmake --preset <preset-name>
cmake --build --preset <build-preset-name>
```

---

## ⚠️ 주의사항

- 모터 명령 CAN ID는 `0x100`입니다.
- 서보 명령 CAN ID는 `0x300`입니다.
- 상태 송신 CAN ID는 `0x200`입니다.
- CAN bitrate는 STM32와 Raspberry Pi에서 반드시 동일해야 합니다.
- 초음파 거리 측정을 위해 TIM2는 1us 단위로 설정하는 것이 좋습니다.
- 서보 PWM은 50Hz 기준으로 설정하는 것이 일반적입니다.
- 현재 안전 정지는 후진 상황에서만 적용됩니다.
- 전진 장애물 감지를 추가하려면 안전 정지 적용 조건을 수정해야 합니다.
- 모터 방향이 반대로 동작하면 `dir_invert` 값 또는 모터 배선을 확인해야 합니다.

---

## 📝 프로젝트 요약

이 펌웨어는 STM32F446을 이용해 로버의 DC 모터, 서보 모터, 초음파 센서, CAN 통신을 통합 제어합니다.

Raspberry Pi 또는 상위 제어기에서 CAN으로 주행 명령과 서보 각도 명령을 보내면 STM32가 이를 수신하여 모터와 서보를 제어합니다.

또한 후진 중 초음파 센서로 장애물을 감지하면 모터 출력을 0으로 제한하여 안전 정지를 수행합니다.
