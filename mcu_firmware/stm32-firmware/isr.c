#include "device_driver.h"
#include <stdio.h>

extern volatile unsigned long g_sys_tick;

// UART2로 들어오는 한 줄(\n까지)을 담는 버퍼 (예: "1 90")
#define RX_LINE_BUF_SIZE 32
volatile char g_rx_line[RX_LINE_BUF_SIZE];
volatile unsigned char g_rx_line_ready = 0; // 1이면 g_rx_line에 파싱 안 된 새 줄이 있음
static unsigned char s_rx_idx = 0;

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

// UART2 수신 인터럽트 ISR (ComPortMaster 등에서 "1 90\n" 형태로 보낸 명령 라인 조립)
void USART2_IRQHandler(void)
{
	unsigned char ch = (unsigned char)(USART2->DR & 0xFF);

	// 이전 줄을 메인 루프가 아직 처리하지 않았으면, 새 입력은 버림(덮어쓰기 방지)
	if (g_rx_line_ready)
		return;

	if (ch == '\r' || ch == '\n')
	{
		if (s_rx_idx > 0) // 빈 줄(엔터만 연속으로 눌림)은 무시
		{
			g_rx_line[s_rx_idx] = '\0';
			g_rx_line_ready = 1;
			s_rx_idx = 0;
		}
	}
	else if (s_rx_idx < (RX_LINE_BUF_SIZE - 1))
	{
		g_rx_line[s_rx_idx++] = (char)ch;
	}
	// 버퍼 꽉 차면 그냥 그 이후 문자는 버림 (다음 개행까지 무시)
}