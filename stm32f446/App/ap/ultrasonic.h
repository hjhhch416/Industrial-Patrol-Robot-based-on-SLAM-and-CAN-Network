// ultrasonic.h
#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * [실전용 HC-SR04 드라이버 요약]
 * - ECHO는 TIM2 Input Capture(예: TIM2_CH1)로 측정 (TIM2는 32-bit)
 * - TRIG는 GPIO로 10us 펄스를 출력
 * - 인터럽트에서는 "캡처 값 저장 + done 플래그 세팅"만 수행(짧고 안전하게)
 * - 메인 루프에서는 trigger/poll을 통해 타임아웃(미수신) 처리 가능
 *
 * [중요]
 * - 본 모듈은 'RAW 측정'만 제공한다.
 * - 필터(median/EMA), 안전정지(연속 N회/히스테리시스)는 상위(ap/safe)에서 적용하는 것이 정석.
 */

typedef enum {
  US_OK = 0,      // 측정 완료(정상)
  US_BUSY,        // 측정 진행 중
  US_TIMEOUT,     // 타임아웃(에코 미수신 또는 비정상)
  US_BAD_PARAM    // 파라미터 오류
} ultrasonic_status_t;

typedef struct {
  // ECHO 측정용 타이머/채널
  TIM_HandleTypeDef *htim;
  uint32_t ch;

  // TRIG 출력용 GPIO
  GPIO_TypeDef *trig_port;
  uint16_t trig_pin;

  // 캡처 상태
  volatile uint32_t t_rise;     // 상승 에지 캡처 타임스탬프
  volatile uint32_t t_fall;     // 하강 에지 캡처 타임스탬프
  volatile uint32_t echo_us;    // 에코 펄스폭(us)  (TIM2 tick=1us일 때)
  volatile uint8_t  got_rise;   // 0: rising 대기 / 1: falling 대기
  volatile uint8_t  done;       // 1: echo_us 갱신 완료

  // 타임아웃 관리(폴링용)
  uint32_t trig_ms;             // trigger 시각(HAL_GetTick)
  uint32_t timeout_ms;          // 타임아웃(ms) (권장: 30~60ms)
} ultrasonic_t;

/*
 * ultrasonic_init()
 * - 구조체 초기화
 * - TIM2 Input Capture 인터럽트 시작
 * - rising 에지부터 측정 시작하도록 폴라리티 설정
 *
 * 사용 조건:
 * - CubeMX에서 TIM2_CHx를 Input Capture로 활성화 + NVIC TIM2 global interrupt enable
 * - TIM2 prescaler를 맞춰 1MHz(1tick=1us)로 세팅하면 echo_us가 직관적
 */
ultrasonic_status_t ultrasonic_init(ultrasonic_t *u,
                                    TIM_HandleTypeDef *htim, uint32_t ch,
                                    GPIO_TypeDef *trig_port, uint16_t trig_pin,
                                    uint32_t timeout_ms);

/*
 * ultrasonic_trigger()
 * - TRIG에 10us 펄스를 출력하여 측정 시작
 * - 내부 상태(done/got_rise) 초기화
 * - trig_ms 갱신(타임아웃 기준점)
 *
 * 권장 트리거 주기: 60~100ms
 */
ultrasonic_status_t ultrasonic_trigger(ultrasonic_t *u, uint32_t now_ms);

/*
 * ultrasonic_poll()
 * - 메인 루프에서 주기적으로 호출하여 측정 완료/타임아웃 상태를 반환
 * - done이면 US_OK, 타임아웃이면 US_TIMEOUT, 그 외는 US_BUSY
 *
 * 참고:
 * - 캡처는 인터럽트에서 처리되지만, '타임아웃/상태 판단'은 폴링에서 처리하는 형태
 */
ultrasonic_status_t ultrasonic_poll(ultrasonic_t *u, uint32_t now_ms);

/*
 * 결과 접근 함수
 */
static inline uint8_t ultrasonic_is_done(const ultrasonic_t *u) { return u ? u->done : 0; }
static inline uint32_t ultrasonic_echo_us(const ultrasonic_t *u) { return u ? u->echo_us : 0; }

/*
 * 거리 변환(근사)
 * - cm ≈ echo_us / 58  (상온 근사식)
 * - 실무에서는 float 출력 문제 때문에 ap에서 정수 기반(us*10/58)로 쓰는 경우가 많음
 */
float ultrasonic_distance_cm_f(const ultrasonic_t *u);

/*
 * ultrasonic_on_capture_irq()
 * - HAL_TIM_IC_CaptureCallback()에서 호출
 * - rising/falling 에지 캡처 & echo_us 계산 & done=1 설정
 *
 * ISR에서는 절대 printf/motor 제어/긴 연산 금지!
 */
void ultrasonic_on_capture_irq(ultrasonic_t *u, TIM_HandleTypeDef *htim);

#endif

/*
 * [초음파(ultrasonic) 모듈 사용 흐름 - 10줄 요약]
 * 1) apInit()에서 ultrasonic_init(&us, &htim2, TIM_CHANNEL_1, TRIG_PORT, TRIG_PIN, timeout_ms) 1회 호출
 * 2) main.c의 HAL_TIM_IC_CaptureCallback() -> apUltrasonic_OnCapture(htim)로 연결
 * 3) apUltrasonic_OnCapture() 내부에서 ultrasonic_on_capture_irq(&us, htim)만 호출(짧게!)
 * 4) apMain() 루프에서 일정 주기(권장 60~100ms)로 ultrasonic_trigger(&us, now_ms) 호출
 * 5) 같은 루프에서 ultrasonic_poll(&us, now_ms)로 상태 확인(US_BUSY/US_OK/US_TIMEOUT)
 * 6) US_OK이면 echo_us = ultrasonic_echo_us(&us)로 결과 읽기(done 플래그는 필요 시 클리어)
 * 7) echo_us는 us(마이크로초) 단위 펄스폭이며, 거리 변환/필터는 상위(safe_distance/ap)에서 처리
 * 8) US_TIMEOUT이면 센서 미수신/비정상 → 안전 정책에 따라 정지/무시 등 처리
 * 9) ISR(캡처 콜백)에서는 printf/모터제어/긴 연산 금지(값 저장만)
 * 10) 최종 모터 출력은 safe_distance_apply_cmd(...)로 게이트한 후 motor_set*(...)로 적용
 */