// ultrasonic.c
#include "ultrasonic.h"

/* ============================================================
 * DWT 기반 us 딜레이
 * - TRIG 10us 펄스는 정확도가 중요하므로 DWT(CYCCNT) 사용을 권장
 * - Cortex-M4(F446)는 DWT 사용 가능
 * ============================================================ */
static uint8_t dwt_ready = 0;

static void dwt_init_once(void)
{
  // 이미 초기화되어 있으면 재초기화하지 않음(중복 설정 방지)
  if (dwt_ready) return;

  // 트레이스 기능 활성화
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // 사이클 카운터 초기화 및 활성화
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // 초기화 완료 플래그
  dwt_ready = 1;
}

static void dwt_delay_us(uint32_t us)
{
  // DWT가 준비 안 되어 있으면 1회 초기화
  dwt_init_once();

  // 시작 시점의 CPU 사이클 카운터 값
  const uint32_t start = DWT->CYCCNT;
  // 목표 지연에 필요한 사이클 수 계산
  const uint32_t ticks = us * (SystemCoreClock / 1000000U);

  while ((DWT->CYCCNT - start) < ticks) {
    // 완전한 빈 루프보다 약간 더 안전한 NOP
    __NOP();
  }
}
/* ============================================================ */

ultrasonic_status_t ultrasonic_init(ultrasonic_t *u,
                                    TIM_HandleTypeDef *htim, uint32_t ch,
                                    GPIO_TypeDef *trig_port, uint16_t trig_pin,
                                    uint32_t timeout_ms)
{
  // ---- 파라미터 유효성 검사 ----
  if (!u || !htim || !trig_port) return US_BAD_PARAM;

  // ---- 하드웨어 리소스 바인딩 ----
  u->htim = htim;
  u->ch = ch;
  u->trig_port = trig_port;
  u->trig_pin  = trig_pin;

  // ---- 측정 상태 초기화 ----
  u->t_rise = 0;
  u->t_fall = 0;
  // 최종 결과
  u->echo_us = 0;
  // 상태 플래그
  u->got_rise = 0;
  u->done = 0;

  // ---- Input Capture 인터럽트 시작 ----
  u->trig_ms = 0;
  u->timeout_ms = timeout_ms;

  // Input Capture 인터럽트 시작 (ECHO 에지 캡처)
  HAL_TIM_IC_Start_IT(u->htim, u->ch);

  // 첫 캡처는 rising 에지(펄스 시작)부터
  __HAL_TIM_SET_CAPTUREPOLARITY(u->htim, u->ch, TIM_INPUTCHANNELPOLARITY_RISING);

  return US_OK;
}

ultrasonic_status_t ultrasonic_trigger(ultrasonic_t *u, uint32_t now_ms)
{
  if (!u) return US_BAD_PARAM;

  // 새로운 측정 시작: 상태 초기화
  u->done = 0;
  u->got_rise = 0;
  u->trig_ms = now_ms;

  // 혹시 이전 측정에서 falling으로 남아있었을 수 있으니 rising으로 복귀
  __HAL_TIM_SET_CAPTUREPOLARITY(u->htim, u->ch, TIM_INPUTCHANNELPOLARITY_RISING);

  // TRIG 펄스 생성: 10us HIGH
  // (데이터시트 권장: 최소 10us)
  HAL_GPIO_WritePin(u->trig_port, u->trig_pin, GPIO_PIN_RESET);
  dwt_delay_us(2);

  HAL_GPIO_WritePin(u->trig_port, u->trig_pin, GPIO_PIN_SET);
  dwt_delay_us(10);

  HAL_GPIO_WritePin(u->trig_port, u->trig_pin, GPIO_PIN_RESET);

  return US_OK;
}

ultrasonic_status_t ultrasonic_poll(ultrasonic_t *u, uint32_t now_ms)
{
  if (!u) return US_BAD_PARAM;

  // 이미 측정 완료
  if (u->done) return US_OK;

  // 아직 trigger를 한 번도 안 했으면 BUSY로 둠 (상위에서 trigger 주기 관리)
  if (u->trig_ms == 0) return US_BUSY;

  // 타임아웃 검사
  if ((now_ms - u->trig_ms) > u->timeout_ms) {
    // 타임아웃 발생: 다음 측정을 위해 상태 초기화
    u->done = 0;
    u->got_rise = 0;
    __HAL_TIM_SET_CAPTUREPOLARITY(u->htim, u->ch, TIM_INPUTCHANNELPOLARITY_RISING);
    return US_TIMEOUT;
  }

  return US_BUSY;
}

float ultrasonic_distance_cm_f(const ultrasonic_t *u)
{
  // cm ≈ us / 58 (상온 근사)
  return (float)ultrasonic_echo_us(u) / 58.0f;
}

void ultrasonic_on_capture_irq(ultrasonic_t *u, TIM_HandleTypeDef *htim)
{
  // 다른 타이머 인터럽트면 무시
  if (!u || htim != u->htim) return;

  // 캡처된 타이머 카운트 값 읽기
  const uint32_t cap = HAL_TIM_ReadCapturedValue(htim, u->ch);

  if (!u->got_rise) {
    // 1) Rising edge: ECHO High 시작 시점
    u->t_rise = cap;
    u->got_rise = 1;

    // 다음은 Falling edge를 캡처하도록 설정
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, u->ch, TIM_INPUTCHANNELPOLARITY_FALLING);
  } else {
    // 2) Falling edge: ECHO High 종료 시점
    u->t_fall = cap;

    // TIM2는 32-bit이므로 wrap 계산을 0xFFFFFFFF 기준으로 처리
    if (u->t_fall >= u->t_rise) {
      u->echo_us = u->t_fall - u->t_rise;
    } else {
      u->echo_us = (0xFFFFFFFFu - u->t_rise) + u->t_fall + 1u;
    }

    // 측정 완료
    u->done = 1;
    u->got_rise = 0;

    // 다음 측정을 위해 rising으로 복귀
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, u->ch, TIM_INPUTCHANNELPOLARITY_RISING);
  }
}