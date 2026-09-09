#ifndef __TIMER_H__
#define __TIMER_H__

// 현재 코드 어디서도 참조되지 않음 - 예전/다른 목적 상수로 보임, 참고용으로만 남김
#define TIM_TICK (20U)
#define TIM_FREQ (1000000.0 / TIM_TICK)
#define TIM_1MS_PLS (TIM_FREQ / 1000.0)

void Timer_Init(void);
void SysTick_1ms_Init(void);

void TIM3_PWM4_Init(void);
void TIM3_PWM_Set_Pulse(unsigned char ch, unsigned short pulse_us);

#endif
