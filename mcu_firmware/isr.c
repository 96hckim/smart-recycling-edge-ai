// =====================================================================
// isr.c
// -----------------------------------------------------------------
// 인터럽트 서비스 루틴(ISR, Interrupt Service Routine) 모음.
// ISR은 "정해진 이벤트(1ms 경과, UART로 문자 도착 등)가 발생하면
// CPU가 하던 작업을 멈추고 자동으로 실행하는 함수"로, 함수 이름은
// 벡터표(스타트업 코드/링커 스크립트)에 미리 정해져 있어서 이 이름
// 그대로 정의만 하면 자동으로 연결됨(오버라이드).
// =====================================================================
#include "device_driver.h"
#include <stdio.h>

// timer.c에 정의된 전역 1ms 카운터 (SysTick_Handler가 여기서 증가시킴)
extern volatile unsigned long g_sys_tick;

// main.c가 UART로 들어온 "한 줄 명령"을 꺼내갈 수 있도록 공유하는 버퍼.
// USART2_IRQHandler(아래)가 한 글자씩 채워서 완성하고, main.c가 다 읽으면 비움.
#define RX_LINE_BUF_SIZE 32
volatile char g_rx_line[RX_LINE_BUF_SIZE];
volatile unsigned char g_rx_line_ready = 0; // 1이면 g_rx_line에 파싱 안 된 새 줄이 있음
static unsigned char s_rx_idx = 0;          // 현재까지 g_rx_line에 채운 글자 수(인덱스)

// 정의되지 않은(처리 함수가 없는) 인터럽트/예외가 발생했을 때 호출되는 안전장치.
// 어떤 인터럽트 번호였는지 출력하고 무한루프에 빠져서, 원인 모를 오동작 대신
// 디버깅에 필요한 정보를 남기고 확실히 멈추게 함.
void _Invalid_ISR(void)
{
	// SCB->ICSR 레지스터 하위 9비트(VECTACTIVE) = 현재 실행 중인 예외/인터럽트 번호
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16); // Cortex-M 예외 번호 16을 빼면 외부 IRQ 번호가 됨
	for (;;)
		;
}

// 1ms 주기 시스템 틱 ISR (Cortex-M4 내장 SysTick 코어 타이머 사용)
// -> SysTick은 읽기만 해도 COUNTFLAG가 클리어되고 NVIC pending도 자동 처리되므로 별도 클리어 불필요
// 이 함수가 1ms마다 호출되어 g_sys_tick을 1씩 늘려주는 것이, main.c의 "200ms마다
// 거리 측정" 로직과 recycle.c의 Delay_ms()가 시간을 재는 유일한 기준이 됨.
void SysTick_Handler(void)
{
	g_sys_tick++;
}

// UART2 수신 인터럽트 ISR (ComPortMaster 등에서 "1 90\n" 형태로 보낸 명령 라인 조립)
// USART2 하드웨어가 문자를 1개 받을 때마다 이 함수가 호출됨. 여기서 하는 일은
// "받은 문자 1개를 g_rx_line 버퍼에 쌓다가, 줄바꿈 문자를 받으면 그 줄을
// 완성된 것으로 표시(g_rx_line_ready=1)"하는 것 뿐 - 실제 명령 해석(파싱)은
// main.c의 Handle_Servo_Command()가 메인 루프에서 처리함(ISR은 최대한 짧게 유지).
void USART2_IRQHandler(void)
{
	unsigned char ch = (unsigned char)(USART2->DR & 0xFF); // DR을 읽으면 수신 플래그(RXNE)도 자동 클리어됨

	// 이전 줄을 메인 루프가 아직 처리하지 않았으면, 새 입력은 버림(덮어쓰기 방지)
	if (g_rx_line_ready)
		return;

	if (ch == '\r' || ch == '\n')
	{
		if (s_rx_idx > 0) // 빈 줄(엔터만 연속으로 눌림)은 무시
		{
			g_rx_line[s_rx_idx] = '\0'; // 문자열 끝을 표시(널 종료)
			g_rx_line_ready = 1;        // main.c에게 "새 줄 완성됐다" 신호
			s_rx_idx = 0;               // 다음 줄을 위해 인덱스 초기화
		}
	}
	else if (s_rx_idx < (RX_LINE_BUF_SIZE - 1))
	{
		g_rx_line[s_rx_idx++] = (char)ch; // 버퍼에 한 글자 추가
	}
	// 버퍼 꽉 차면 그냥 그 이후 문자는 버림 (다음 개행까지 무시)
}
