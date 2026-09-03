// =====================================================================
// timer.h
// -----------------------------------------------------------------
// 이 프로젝트에서 쓰는 타이머 관련 초기화 함수 선언.
// - SysTick(코어 내장 1ms 틱) : main.c의 200ms 주기 측정, recycle.c의
//   Delay_ms()가 참조하는 g_sys_tick을 1ms마다 증가시킴
// - TIM3 : 서보 4개(PC6~PC9)를 구동하는 50Hz PWM 전용 타이머
// (초음파 센서용 TIM4는 여기 없고 ultrasonic.c 안에서 직접 초기화함 - Ultra_Init())
// =====================================================================

#ifndef __TIMER_H__
#define __TIMER_H__

// TIM_TICK/TIM_FREQ/TIM_1MS_PLS: 현재 소스 어디에서도 실제로 사용되지 않는
// 예전(혹은 다른 목적) 상수로 보임 - 참고용으로만 남아있음
#define TIM_TICK (20U)
#define TIM_FREQ (1000000.0 / TIM_TICK)
#define TIM_1MS_PLS (TIM_FREQ / 1000.0)

void Timer_Init(void);        // SysTick_1ms_Init() + TIM3_PWM4_Init()를 한 번에 호출
void SysTick_1ms_Init(void);  // Cortex-M4 내장 SysTick을 1ms 주기 인터럽트로 설정

void TIM3_PWM4_Init(void);                                          // 서보 4개용 PWM (50Hz, PC6~PC9)
void TIM3_PWM_Set_Pulse(unsigned char ch, unsigned short pulse_us);  // ch: 0~3, 채널별 펄스 폭(us) 설정 -> 서보 각도 결정

#endif // __TIMER_H__
