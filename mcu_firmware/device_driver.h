// =====================================================================
// device_driver.h
// -----------------------------------------------------------------
// 이 프로젝트의 거의 모든 .c 파일 맨 위에 #include 되는 "공용 헤더".
// - stm32f4xx.h  : 벤더(ST) 제공, 모든 레지스터 이름(RCC, GPIOC, USART2, TIM3 ...)이
//                  정의되어 있는 헤더 (건드릴 필요 없음)
// - option.h     : 클럭/메모리 설정값 (위에서 설명)
// - macro.h      : 레지스터 비트 조작 매크로 (위에서 설명)
// - malloc.h     : 표준 C 동적 메모리 할당 함수 선언
// 이렇게 한 곳에 모아두면 각 .c 파일은 이 헤더 하나만 include해서
// 레지스터 이름 + 매크로 + 설정값을 전부 쓸 수 있음.
//
// 그리고 clock.c / uart.c 에 정의된 함수들을 다른 파일(main.c 등)에서도
// 호출할 수 있도록 여기서 extern 선언을 해둠 (실제 구현은 각 .c 파일에 있음).
// =====================================================================

#ifndef DEVICE_DRIVER_H
#define DEVICE_DRIVER_H

#include "stm32f4xx.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"

// 시스템 및 클럭 초기화 (구현: clock.c)
extern void Clock_Init(void);

// UART2 드라이버 (Qt GUI 통신용) (구현: uart.c)
extern void Uart2_Init(int baud);              // UART2 초기화 (통신 속도 baud로 설정)
extern void Uart2_RX_Interrupt_Enable(int en);  // UART2 수신 인터럽트 켜기(1)/끄기(0)

#endif // DEVICE_DRIVER_H
