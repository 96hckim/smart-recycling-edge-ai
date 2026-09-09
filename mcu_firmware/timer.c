#include "device_driver.h"
#include "timer.h"

volatile unsigned long g_sys_tick = 0;

// main.c가 부팅 시 한 번만 호출하도록 두 타이머 초기화를 한데 묶은 진입점
void Timer_Init(void)
{
	SysTick_1ms_Init();
	TIM3_PWM4_Init();
}

// TIM4를 안 쓰고 코어 내장 SysTick을 쓰는 이유: TIM4는 초음파 센서(ultrasonic.c)의
// 1us 프리런 카운터로 이미 쓰이고 있어서 1ms 틱은 별도 타이머로 분리함
void SysTick_1ms_Init(void)
{
	SysTick_Config(SYSCLK / 1000U);
}

// 서보는 20ms(50Hz) 주기마다 0.5~2.5ms 펄스 폭으로 각도를 받는 규격이라 그 PWM을 만든다
// 실사용 서보가 3개(PC6~PC8)뿐이라 CH4(PC9)는 세팅하지 않음
void TIM3_PWM4_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 2U);

	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 12U);
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 14U);
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 16U);

	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 24U);
	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 28U);
	Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, 0U);

	Macro_Set_Bit(RCC->APB1ENR, 1U);

	// PSC로 카운터 1틱=1us가 되게 맞춰서, ARR=19999 -> 20000us(50Hz) 주기가 되도록
	TIM3->PSC = (unsigned int)(TIMXCLK / 1000000.0 + 0.5) - 1U;
	TIM3->ARR = 20000U - 1U;

	// PWM 모드1 + 프리로드: CCRx 값이 곧 "몇 us 동안 HIGH 유지할지"가 됨
	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 4U);
	Macro_Set_Bit(TIM3->CCMR1, 3U);
	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 12U);
	Macro_Set_Bit(TIM3->CCMR1, 11U);
	Macro_Write_Block(TIM3->CCMR2, 0x7, 0x6, 4U);
	Macro_Set_Bit(TIM3->CCMR2, 3U);

	Macro_Set_Bit(TIM3->CCER, 0U);
	Macro_Set_Bit(TIM3->CCER, 4U);
	Macro_Set_Bit(TIM3->CCER, 8U);

	// 위 설정값들은 섀도우 레지스터에 있어 강제 업데이트 이벤트로 즉시 반영시켜야 함
	Macro_Set_Bit(TIM3->EGR, 0U);
	Macro_Clear_Bit(TIM3->SR, 0U);

	Macro_Set_Bit(TIM3->CR1, 7U);
	Macro_Set_Bit(TIM3->CR1, 0U);
}

// PSC가 1틱=1us라 CCRx에 pulse_us를 그대로 넣으면 됨 (servo.c가 각도->펄스 변환 후 호출)
void TIM3_PWM_Set_Pulse(unsigned char ch, unsigned short pulse_us)
{
	switch (ch)
	{
	case 0: TIM3->CCR1 = pulse_us; break;
	case 1: TIM3->CCR2 = pulse_us; break;
	case 2: TIM3->CCR3 = pulse_us; break;
	default: break;
	}
}
