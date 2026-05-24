#include "ap.h"
#include <stdint.h>
#include <stdio.h>  // sprintf 사용을 위해
#include <string.h> // strlen 사용을 위해

volatile uint16_t adc_values[3];

void apInit(void) {
  // ADC를 DMA 모드로 시작 (버퍼 주소와 크기 전달)
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_values, 3) != HAL_OK) {
   Error_Handler();
  }
}


void apMain(void) {
  char tx_buffer[50]; // 출력 버퍼

  while (1) {
    // X축 값 출력
    int len_x = sprintf(tx_buffer, ">x:%u\r\n", adc_values[0]);
    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, len_x, 100);

    // Y축 값 출력
    int len_y = sprintf(tx_buffer, ">y:%u\r\n", adc_values[1]);
    HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, len_y, 100);

    HAL_Delay(50); // 0.5초 대기 (너무 빠른 출력을 방지)

  }
}