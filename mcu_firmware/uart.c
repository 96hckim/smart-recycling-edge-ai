#include "device_driver.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// USART2(PA2/PA3)가 실제 통신 채널: printf도 syscalls.c _write()를 거쳐 여기로 나감.
// USART1은 예비/디버깅용 폴링 포트라 인터럽트 없이 blocking 함수만 둠.
void Uart2_Init(int baud)
{
  double div;
  unsigned int mant;
  unsigned int frac;

  Macro_Set_Bit(RCC->AHB1ENR, 0);
  Macro_Set_Bit(RCC->APB1ENR, 17);

  Macro_Write_Block(GPIOA->MODER, 0xf, 0xa, 4);
  Macro_Write_Block(GPIOA->AFR[0], 0xff, 0x77, 8);
  Macro_Write_Block(GPIOA->PUPDR, 0xf, 0x5, 4);

  // GPIO Lock 키 시퀀스(0->1->0->1, 마지막에 읽기)로 PA2/3 설정이 실수로 재변경되는 걸 방지
  volatile unsigned int t = GPIOA->LCKR & 0x7FFF;
  GPIOA->LCKR = (0x1 << 16) | t | (0x3 << 2);
  GPIOA->LCKR = (0x0 << 16) | t | (0x3 << 2);
  GPIOA->LCKR = (0x1 << 16) | t | (0x3 << 2);
  t = GPIOA->LCKR;

  div = PCLK1 / (16. * baud);
  mant = (int)div;
  frac = (int)((div - mant) * 16. + 0.5);
  mant += frac >> 4; // 소수부 반올림이 16을 넘으면 정수부로 carry
  frac &= 0xf;

  USART2->BRR = (mant << 4) | (frac << 0);

  USART2->CR1 = (1 << 13) | (0 << 12) | (0 << 10) | (1 << 3) | (1 << 2);
  USART2->CR2 = 0 << 12;
  USART2->CR3 = 0;
}

// Uart2_Init과 구조 동일하지만 USART1은 APB2 버스(PCLK2)라 보레이트 계산 클럭이 다름
void Uart1_Init(int baud)
{
  double div;
  unsigned int mant;
  unsigned int frac;

  Macro_Set_Bit(RCC->AHB1ENR, 0);
  Macro_Set_Bit(RCC->APB2ENR, 4);
  Macro_Write_Block(GPIOA->MODER, 0xf, 0xa, 18);
  Macro_Write_Block(GPIOA->AFR[1], 0xff, 0x77, 4);
  Macro_Write_Block(GPIOA->PUPDR, 0xf, 0x5, 18);

  div = PCLK2 / (16. * baud);
  mant = (int)div;
  frac = (int)((div - mant) * 16 + 0.5);
  mant += frac >> 4;
  frac &= 0xf;
  USART1->BRR = (mant << 4) | (frac << 0);

  USART1->CR1 = (1 << 13) | (0 << 12) | (0 << 10) | (1 << 3) | (1 << 2);
  USART1->CR2 = 0 << 12;
  USART1->CR3 = 0;
}

// '\n' 앞에 '\r'을 끼워 "\r\n"으로 맞춤 - 터미널에서 줄바꿈 깨짐 방지
void Uart1_Send_Byte(char data)
{
  if (data == '\n')
  {
    while (!Macro_Check_Bit_Set(USART1->SR, 7))
      ;
    USART1->DR = 0x0d;
  }

  while (!Macro_Check_Bit_Set(USART1->SR, 7))
    ;
  USART1->DR = data;
}

// 데이터 들어올 때까지 블로킹 - Get_Pressed와 짝을 이루는 대기형 버전
char Uart1_Get_Char(void)
{
  while (!Macro_Check_Bit_Set(USART1->SR, 5))
    ;
  return (char)USART1->DR;
}

// Get_Char와 달리 데이터 없으면 즉시 0 반환 - 대기 없이 눌림 여부만 폴링할 때용
char Uart1_Get_Pressed(void)
{
  if (Macro_Check_Bit_Set(USART1->SR, 5))
  {
    return (char)USART1->DR;
  }

  else
  {
    return (char)0;
  }
}

// main.c는 이걸 켜서 씀 - 바이트 수신마다 isr.c의 USART2_IRQHandler가 돌게 됨.
// NVIC(38)은 STM32F411 벡터표상 USART2 라인 번호.
void Uart2_RX_Interrupt_Enable(int en)
{
  if (en)
  {
    Macro_Set_Bit(USART2->CR1, 5);
    NVIC_ClearPendingIRQ(38); // 켜기 전 남아있던 pending 인터럽트 정리
    NVIC_EnableIRQ(38);
  }
  else
  {
    Macro_Clear_Bit(USART2->CR1, 5);
    NVIC_DisableIRQ(38);
  }
}
