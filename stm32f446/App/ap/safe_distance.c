// safe_distance.c
#include "safe_distance.h"

/* 3개 값의 중간값(median) 구하기: 스파이크(한 번 튐)에 강함 */
static uint32_t median3_u32(uint32_t a, uint32_t b, uint32_t c) {
  if (a > b) {
    uint32_t t = a;
    a = b;
    b = t;
  }
  if (b > c) {
    uint32_t t = b;
    b = c;
    c = t;
  }
  if (a > b) {
    uint32_t t = a;
    a = b;
    b = t;
  }
  return b;
}

// 안전 정지(ESTOP) 상태로
static void enter_estop(safe_distance_t *sd) {
  sd->state = SAFE_ESTOP; // 현재 상태를 ESTOP으로 설정
  sd->stop_cnt = 0;       // stop 연속 카운터 리셋(이전 누적값 무효화)
  sd->rel_cnt = 0;        // release 연속 카운터도 0부터 다시 시작
}

// 정상 동작(RUN) 상태로
static void enter_run(safe_distance_t *sd) {
  sd->state = SAFE_RUN; // 현재 상태를 RUN으로 설정
  sd->stop_cnt = 0;     // stop 연속 카운터 리셋(0부터 다시 카운트)
  sd->rel_cnt = 0;      // release 연속 카운터 리셋
}

/* EMA: y = y + alpha*(x-y)  (alpha는 Q15: 0~32768(1.0)) */
static uint32_t ema_u32_q15(uint32_t y, uint32_t x, uint16_t alpha_q15) {
  // alpha_q15=0이면 변화 없음
  // alpha_q15=32768이면 즉시 x로 점프
  int32_t dy = (int32_t)x - (int32_t)y;
  int32_t step = (dy * (int32_t)alpha_q15) >> 15;
  return (uint32_t)((int32_t)y + step);
}

void safe_distance_init(safe_distance_t *sd, uint16_t stop_cm,
                        uint16_t release_cm, uint8_t stop_count_n,
                        uint8_t release_count_n, uint32_t sensor_timeout_ms) {
  sd->stop_cm = stop_cm;           // 이 거리 이하이면 정지 후보
  sd->release_cm = release_cm;     // 이 거리 이상이면 해제 후보(히스테리시스)
  sd->stop_count_n = stop_count_n; // 연속 N회 stop 만족 시 ESTOP
  sd->release_count_n = release_count_n; // 연속 N회 release 만족 시 RUN 복귀
  sd->sensor_timeout_ms =
      sensor_timeout_ms; // 마지막 유효 측정 이후 이 시간 넘으면 ESTOP

  // 기본 게이트 범위(2cm~400cm 근사)
  sd->echo_us_min = 120;
  sd->echo_us_max = 25000;

  // 기본: EMA 끔(0). 필요하면 set_filter로 켜기
  sd->ema_alpha_q15 = 0;

  sd->state = SAFE_RUN;
  sd->stop_cnt = 0;
  sd->rel_cnt = 0;

  sd->hist_us[0] = sd->hist_us[1] = sd->hist_us[2] = 0;
  sd->hist_n = 0;

  sd->dist_1dp = 0;
  sd->dist_1dp_ema = 0;

  sd->last_update_ms = 0;
  sd->has_measure = 0;
}

void safe_distance_set_filter(safe_distance_t *sd, uint32_t echo_us_min,
                              uint32_t echo_us_max, uint16_t ema_alpha_q15) {
  sd->echo_us_min = echo_us_min;     // 유효 echo_us 최소값(범위 게이트 하한)
  sd->echo_us_max = echo_us_max;     // 유효 echo_us 최대값(범위 게이트 상한)
  sd->ema_alpha_q15 = ema_alpha_q15; // EMA 강도(Q15). 0이면 EMA 사용 안 함
}

uint8_t safe_distance_feed_echo_us(safe_distance_t *sd, uint32_t now_ms,
                                   uint32_t echo_us) {
  // 1) 범위 게이트: 말도 안 되는 값은 안전하게 버림
  if (echo_us < sd->echo_us_min || echo_us > sd->echo_us_max) {
    return 0;
  }

  // 2) 3-sample median: 스파이크 제거
  sd->hist_us[sd->hist_n % 3] = echo_us;

  uint32_t us_med;
  if (sd->hist_n < 3) {
    // 초기 3개가 모이기 전: 카운트 증가 & raw 데이터 그대로 사용
    sd->hist_n++;
    us_med = echo_us;
  } else {
    // 3개가 다 모인 후: 오버플로우 방지를 위해 3, 4, 5 구간에서만 순환
    sd->hist_n = 3 + ((sd->hist_n + 1) % 3);
    us_med = median3_u32(sd->hist_us[0], sd->hist_us[1], sd->hist_us[2]);
  }

  // 3) 거리 변환(0.1cm 단위)
  // cm ≈ us/58  => (cm*10) ≈ (us*10)/58
  uint32_t dist_1dp = (us_med * 10U) / 58U;

  // 4) (선택) EMA로 부드럽게
  if (sd->ema_alpha_q15 != 0) {
    if (!sd->has_measure)
      sd->dist_1dp_ema = dist_1dp; // 첫 값은 바로 세팅
    else
      sd->dist_1dp_ema =
          ema_u32_q15(sd->dist_1dp_ema, dist_1dp, sd->ema_alpha_q15);

    sd->dist_1dp = sd->dist_1dp_ema;
  } else {
    sd->dist_1dp = dist_1dp;
  }

  // 5) 유효 측정 갱신
  sd->last_update_ms = now_ms;
  sd->has_measure = 1;

  return 1;
}

void safe_distance_apply_cmd(safe_distance_t *sd, int32_t left_in,
                             int32_t right_in, uint32_t now_ms,
                             int32_t *left_out, int32_t *right_out) {
  // 기본은 입력 그대로
  *left_out = left_in;
  *right_out = right_in;

  // (A) 센서 타임아웃/미측정이면 안전상 ESTOP
  if (!sd->has_measure) {
    enter_estop(sd);
  } else {
    uint32_t dt = now_ms - sd->last_update_ms;
    if (dt > sd->sensor_timeout_ms) {
      enter_estop(sd);
    }
  }

  // (B) 거리 기반 안전 판정(히스테리시스 + 연속 N회)
  if (sd->has_measure) {
    // 0.1cm -> cm 정수
    uint32_t dist_cm = sd->dist_1dp / 10U;

    if (sd->state == SAFE_RUN) {
      // STOP 조건: dist <= stop_cm 연속 N회
      if (dist_cm <= sd->stop_cm) {
        sd->stop_cnt++;
        if (sd->stop_cnt >= sd->stop_count_n) {
          enter_estop(sd);
        }
      } else {
        sd->stop_cnt = 0;
      }
    } else { // SAFE_ESTOP
      // RELEASE 조건: dist >= release_cm 연속 N회
      if (dist_cm >= sd->release_cm) {
        sd->rel_cnt++;
        if (sd->rel_cnt >= sd->release_count_n) {
          enter_run(sd);
        }
      } else {
        sd->rel_cnt = 0;
      }
    }
  }

  // (C) 최종 게이팅: ESTOP이면 무조건 정지(명령 0)
  if (sd->state == SAFE_ESTOP) {
    *left_out = 0;
    *right_out = 0;
  }
}

// 현재 안전 상태를 반환하는 조회 함수(getter)
safe_state_t safe_distance_get_state(const safe_distance_t *sd) {
  return sd->state;
}

// safe_distance 내부에서 최종 필터링된 거리 값을 반환하는 조회 함수
uint32_t safe_distance_get_dist_1dp(const safe_distance_t *sd) {
  return sd->dist_1dp;
}