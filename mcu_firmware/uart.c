// =====================================================================
// uart.c
// -----------------------------------------------------------------
// USART1/USART2 두 개의 시리얼 포트를 초기화/송수신하는 드라이버.
// - USART2 (PA2=TX, PA3=RX) : 이 프로젝트의 실제 통신 채널.
//   PC의 ComPortMaster(또는 Qt GUI) <-> 보드 간 명령/로그를 주고받는 데 사용.
//   main.c의 printf()도 결국 syscalls.c의 _write()를 거쳐 이 USART2로 나감.
//   수신은 인터럽트 방식(isr.c의 USART2_IRQHandler)으로 처리.
// - USART1 (PA9=TX, PA10=RX) : 폴링(대기) 방식 송수신 함수만 제공되는
//   보조 포트. 이 프로젝트의 main.c에서는 실제로 사용하지 않음(예비/디버깅용).
// =====================================================================
#include "device_driver.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// USART2 초기화: GPIO를 UART 기능으로 설정하고, 지정한 baud(통신 속도)로 통신하도록
// BRR(보레이트 레지스터)을 계산해서 넣고, 송수신을 켠다.
void Uart2_Init(int baud)
{
  double div;
  unsigned int mant; // 정수부 (Mantissa)
  unsigned int frac;  // 소수부 (Fraction, 4비트로 표현)

  // 1) 클럭 공급: GPIOA(핀 사용) + USART2(APB1 버스, PCLK1=48MHz)
  Macro_Set_Bit(RCC->AHB1ENR, 0);                  // PA2,3
  Macro_Set_Bit(RCC->APB1ENR, 17);                 // USART2 ON

  // 2) PA2(TX), PA3(RX)를 대체기능(AF) 모드로, AF07(=USARTx 기능)로, 풀업으로 설정
  Macro_Write_Block(GPIOA->MODER, 0xf, 0xa, 4);    // PA2,3 => ALT   (핀당 2비트: 0b10=AF)
  Macro_Write_Block(GPIOA->AFR[0], 0xff, 0x77, 8); // PA2,3 => AF07  (핀당 4비트: 0x7=AF7=USART2)
  Macro_Write_Block(GPIOA->PUPDR, 0xf, 0x5, 4);    // PA2,3 => Pull-Up (핀당 2비트: 0b01=풀업)

  // 3) 위에서 설정한 PA2,3 핀 구성을 실수로 다시 바뀌지 않게 잠그는(Lock) 절차.
  //    STM32 GPIO Lock 메커니즘 정해진 순서: 0->1->0(또는1)->1 순으로 LCKR에 써야
  //    실제로 잠김(데이터시트에 나오는 "락 키 시퀀스"). 여기서는 PA2,3(bit2,3)만 잠금.
  volatile unsigned int t = GPIOA->LCKR & 0x7FFF;
  GPIOA->LCKR = (0x1 << 16) | t | (0x3 << 2); // Lock PA2, 3 Configuration
  GPIOA->LCKR = (0x0 << 16) | t | (0x3 << 2);
  GPIOA->LCKR = (0x1 << 16) | t | (0x3 << 2);
  t = GPIOA->LCKR; // (락 시퀀스 마지막에 LCKR을 한 번 읽어줘야 확정됨)

  // 4) 보레이트 계산: STM32 USART의 BRR = (정수부 << 4) | 소수부(4비트)
  //    USARTDIV = PCLK1 / (16 * baud) (오버샘플링 16배 기준)
  //    소수부는 반올림 후 0~15 범위를 넘으면 정수부로 올림(carry) 처리
  div = PCLK1 / (16. * baud);
  mant = (int)div;
  frac = (int)((div - mant) * 16. + 0.5);
  mant += frac >> 4;  // 소수부 반올림으로 16이 되면 정수부에 1 carry
  frac &= 0xf;        // 소수부는 4비트만 유지

  USART2->BRR = (mant << 4) | (frac << 0);

  // 5) USART2 동작 모드 설정
  //    CR1: bit13=UE(USART Enable), bit12=M(0=8비트 데이터), bit10=PCE(0=패리티 없음),
  //         bit3=TE(송신 활성화), bit2=RE(수신 활성화)
  USART2->CR1 = (1 << 13) | (0 << 12) | (0 << 10) | (1 << 3) | (1 << 2);
  USART2->CR2 = 0 << 12; // 스톱비트 등 기본값(1 stop bit)
  USART2->CR3 = 0;       // 흐름제어(RTS/CTS) 등 사용 안 함
}

// USART1 초기화 (USART2_Init과 구조는 동일, 핀/버스/레지스터만 USART1용으로 다름)
// PA9=TX, PA10=RX, APB2 버스(PCLK2=96MHz) 기준으로 보레이트 계산
void Uart1_Init(int baud)
{
  double div;
  unsigned int mant;
  unsigned int frac;

  Macro_Set_Bit(RCC->AHB1ENR, 0);                  // PA9,10
  Macro_Set_Bit(RCC->APB2ENR, 4);                  // USART1 ON
  Macro_Write_Block(GPIOA->MODER, 0xf, 0xa, 18);   // PA9,10 => ALT
  Macro_Write_Block(GPIOA->AFR[1], 0xff, 0x77, 4); // PA9,10 => AF07
  Macro_Write_Block(GPIOA->PUPDR, 0xf, 0x5, 18);   // PA9,10 => Pull-Up

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

// USART1로 1바이트 전송 (폴링 방식: 송신 가능해질 때까지 while로 대기).
// '\n'이 오면 관례상 그 앞에 '\r'(캐리지리턴)을 먼저 보내서 "\r\n" 줄바꿈으로 맞춰줌
// (터미널 프로그램에서 줄바꿈이 깨지지 않게 하기 위함).
void Uart1_Send_Byte(char data)
{
  if (data == '\n')
  {
    while (!Macro_Check_Bit_Set(USART1->SR, 7)) // SR bit7 = TXE(송신 버퍼 비어있음)
      ;
    USART1->DR = 0x0d; // '\r' 먼저 전송
  }

  while (!Macro_Check_Bit_Set(USART1->SR, 7))
    ;
  USART1->DR = data;
}

// USART1에서 1바이트 수신 (폴링 방식: 데이터가 들어올 때까지 계속 대기하며 블로킹됨)
char Uart1_Get_Char(void)
{
  while (!Macro_Check_Bit_Set(USART1->SR, 5)) // SR bit5 = RXNE(수신 데이터 있음)
    ;
  return (char)USART1->DR;
}

// USART1에서 이미 들어와 있는 데이터가 있으면 즉시 반환, 없으면 기다리지 않고 0을 반환
// (Uart1_Get_Char와 달리 "논블로킹" - 키 입력이 있었는지만 확인하고 싶을 때 사용)
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

// USART2 수신 인터럽트를 켜거나(en=1) 끈다(en=0).
// 켜면 한 바이트가 들어올 때마다 CPU가 하던 일을 멈추고 isr.c의
// USART2_IRQHandler()를 실행하게 됨 (main.c에서 켜서 사용 중).
// NVIC(38)는 STM32F411 벡터표 상 USART2 인터럽트 번호.
void Uart2_RX_Interrupt_Enable(int en)
{
  if (en)
  {
    Macro_Set_Bit(USART2->CR1, 5);   // CR1 bit5 = RXNEIE (수신 인터럽트 허용)
    NVIC_ClearPendingIRQ(38);        // 혹시 남아있던 대기(pending) 인터럽트 지우고 시작
    NVIC_EnableIRQ(38);              // NVIC(인터럽트 컨트롤러)에서 USART2 인터럽트 라인 활성화
  }
  else
  {
    Macro_Clear_Bit(USART2->CR1, 5);
    NVIC_DisableIRQ(38);
  }
}
