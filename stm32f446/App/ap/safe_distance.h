// safe_distance.h
#ifndef SAFE_DISTANCE_H
#define SAFE_DISTANCE_H

#include <stdint.h>

typedef enum {
  SAFE_RUN = 0,
  SAFE_ESTOP = 1
} safe_state_t;

/*
 * safe_distance_t
 * - 초음파 echo_us(raw)를 받아서
 *   (A) 스파이크 제거용 필터 + (B) 안전 판정(정지/해제)을 내부에서 수행하는 모듈
 *
 * - ap에서는:
 *   1) 측정 완료 시 safe_distance_feed_echo_us(sd, now, echo_us)
 *   2) 모터 출력 직전 safe_distance_apply_cmd(sd, cmdL, cmdR, now, &outL, &outR)
 *   이렇게만 쓰면 됨
 */
typedef struct {
  /* ====== 설정값(튜닝 파라미터) ====== */
  uint16_t stop_cm;           // 이 거리 이하이면 정지 후보
  uint16_t release_cm;        // 이 거리 이상이면 해제 후보(히스테리시스)
  uint8_t  stop_count_n;      // 연속 N회 stop 만족 시 ESTOP
  uint8_t  release_count_n;   // 연속 N회 release 만족 시 RUN 복귀
  uint32_t sensor_timeout_ms; // 마지막 유효 측정 이후 이 시간 넘으면 ESTOP

  // echo_us 유효 범위(게이트): 말도 안 되는 값 버림
  // (2cm~400cm 근사) -> us 기준 대략 120~25000
  uint32_t echo_us_min;
  uint32_t echo_us_max;

  // EMA(지수평활) 사용 여부/강도 (0이면 EMA 사용 안 함)
  // 예: ema_alpha_q15 = 6554  (≈0.2)
  uint16_t ema_alpha_q15;

  /* ====== 내부 상태 ====== */
  safe_state_t state;
  uint8_t stop_cnt;
  uint8_t rel_cnt;

  // 최근 3개 raw(us) 저장 (median용)
  uint32_t hist_us[3];
  uint8_t  hist_n;

  // 필터 결과(단위: 0.1cm)
  // dist_1dp = 거리(cm)*10
  uint32_t dist_1dp;          // 최종(필터/EMA 포함) 결과
  uint32_t dist_1dp_ema;      // EMA 내부 상태(0.1cm)

  // 마지막 유효 측정 시각
  uint32_t last_update_ms;
  uint8_t  has_measure;       // 유효 측정이 한 번이라도 들어왔는지
} safe_distance_t;


/* 초기화: 모든 파라미터 설정 */
void safe_distance_init(safe_distance_t *sd,
                        uint16_t stop_cm,
                        uint16_t release_cm,
                        uint8_t stop_count_n,
                        uint8_t release_count_n,
                        uint32_t sensor_timeout_ms);

/*
 * safe_distance_set_filter()
 * - 게이트 범위와 EMA를 설정(선택)
 * - ema_alpha_q15 = 0이면 EMA 비활성(=median 결과 그대로 사용)
 */
void safe_distance_set_filter(safe_distance_t *sd,
                              uint32_t echo_us_min,
                              uint32_t echo_us_max,
                              uint16_t ema_alpha_q15);

/*
 * safe_distance_feed_echo_us()
 * - 초음파 측정 완료 시 raw echo_us를 넣어준다.
 * - 내부에서:
 *   1) range gate(유효 범위) 적용
 *   2) 3-sample median으로 스파이크 제거
 *   3) cm(0.1cm 단위)로 변환
 *   4) (옵션) EMA로 부드럽게
 *   5) 마지막 업데이트 시각 갱신
 *
 * 반환:
 * - 1: 유효 측정으로 받아들여짐(상태 갱신됨)
 * - 0: 범위 밖이라 버림(상태 갱신 안 함)
 */
uint8_t safe_distance_feed_echo_us(safe_distance_t *sd,
                                   uint32_t now_ms,
                                   uint32_t echo_us);

/*
 * safe_distance_apply_cmd()
 * - 상위에서 만든 원시 명령(cmdL/cmdR)에 안전 게이트를 적용한 결과(outL/outR)를 반환
 * - 내부에서:
 *   1) 센서 타임아웃이면 ESTOP
 *   2) 거리 기반 히스테리시스/연속N회로 ESTOP/RUN 갱신
 *   3) ESTOP이면 out=0으로 강제
 */
void safe_distance_apply_cmd(safe_distance_t *sd,
                             int32_t left_in, int32_t right_in,
                             uint32_t now_ms,
                             int32_t *left_out, int32_t *right_out);

/* 상태/값 조회 */
safe_state_t safe_distance_get_state(const safe_distance_t *sd);

/* 필터된 거리 반환 (단위: 0.1cm). 예: 253 => 25.3cm */
uint32_t safe_distance_get_dist_1dp(const safe_distance_t *sd);

#endif

/*
 * [safe_distance 모듈 사용 흐름 - 10줄 요약]
 * 1) apInit()에서 safe_distance_init(&sd, STOP_CM, RELEASE_CM, STOP_N, RELEASE_N, timeout_ms) 1회 호출
 * 2) (선택) safe_distance_set_filter(&sd, echo_us_min, echo_us_max, ema_alpha_q15)로 게이트/EMA 파라미터 설정
 * 3) 초음파 측정 완료 시(US_OK) echo_us를 safe_distance_feed_echo_us(&sd, now_ms, echo_us)로 입력
 * 4) feed 함수는 내부에서 range gate -> 3-sample median -> (옵션) EMA -> dist_1dp 갱신
 * 5) 모터 출력 직전에 safe_distance_apply_cmd(&sd, cmdL_raw, cmdR_raw, now_ms, &cmdL_safe, &cmdR_safe) 호출
 * 6) apply_cmd는 센서 타임아웃이면 ESTOP 강제(안전상 우선)
 * 7) apply_cmd는 dist_1dp 기반 STOP/RELEASE + 연속 N회 조건으로 RUN/ESTOP 상태 전이
 * 8) 상태가 ESTOP이면 cmdL_safe/cmdR_safe를 무조건 0으로 게이트
 * 9) safe_distance_get_state()/get_dist_1dp()로 상태/거리(0.1cm 단위)를 로그/통신에 사용 가능
 * 10) 최종 모터는 motor_set*(cmd_safe)만 사용(ESTOP는 motor_set_lr(0,0)로 즉시 정지 가능)
 */