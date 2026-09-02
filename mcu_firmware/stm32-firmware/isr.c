#include "device_driver.h"
#include <stdio.h>

extern volatile unsigned long g_sys_tick;

volatile unsigned char g_rx_cmd = '0';
volatile unsigned char g_rx_flag = 0;

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for (;;)
		;
}

// 1ms 주기 시스템 틱 ISR (Cortex-M4 내장 SysTick 코어 타이머 사용)
// -> SysTick은 읽기만 해도 COUNTFLAG가 클리어되고 NVIC pending도 자동 처리되므로 별도 클리어 불필요
void SysTick_Handler(void)
{
	g_sys_tick++;
}

// UART2 수신 인터럽트 ISR (Qt 원격 제어 명령 수신)
void USART2_IRQHandler(void)
{
	g_rx_cmd = (unsigned char)(USART2->DR & 0xFF);
	g_rx_flag = 1;
}