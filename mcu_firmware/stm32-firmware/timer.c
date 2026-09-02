#include "device_driver.h"
#include "timer.h"

volatile unsigned long g_sys_tick = 0;

void Timer_Init(void)
{
	SysTick_1ms_Init();
	TIM3_PWM4_Init();
}

// 1ms 주기 시스템 틱: STM32 범용 타이머(TIM4) 대신 Cortex-M4 내장 SysTick 코어 타이머 사용
// -> TIM4는 초음파 센서(ultrasonic.c)의 1us 프리런 카운터로 재사용
void SysTick_1ms_Init(void)
{
	SysTick_Config(SYSCLK / 1000U); // HCLK 기준 1ms마다 인터럽트
}

// 서보 4개용 50Hz PWM 초기화 (TIM3 CH1~CH4 = PC6, PC7, PC8, PC9)
void TIM3_PWM4_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 2U); // GPIOC 클럭

	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 12U); // PC6 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 14U); // PC7 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 16U); // PC8 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 18U); // PC9 AF

	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 24U); // PC6 -> AF2 (TIM3_CH1)
	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 28U); // PC7 -> AF2 (TIM3_CH2)
	Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, 0U);  // PC8 -> AF2 (TIM3_CH3)
	Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, 4U);  // PC9 -> AF2 (TIM3_CH4)

	Macro_Set_Bit(RCC->APB1ENR, 1U); // TIM3 클럭

	TIM3->PSC = (unsigned int)(TIMXCLK / 1000000.0 + 0.5) - 1U;
	TIM3->ARR = 20000U - 1U;

	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 4U);
	Macro_Set_Bit(TIM3->CCMR1, 3U);
	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 12U);
	Macro_Set_Bit(TIM3->CCMR1, 11U);
	Macro_Write_Block(TIM3->CCMR2, 0x7, 0x6, 4U);
	Macro_Set_Bit(TIM3->CCMR2, 3U);
	Macro_Write_Block(TIM3->CCMR2, 0x7, 0x6, 12U);
	Macro_Set_Bit(TIM3->CCMR2, 11U);

	Macro_Set_Bit(TIM3->CCER, 0U);
	Macro_Set_Bit(TIM3->CCER, 4U);
	Macro_Set_Bit(TIM3->CCER, 8U);
	Macro_Set_Bit(TIM3->CCER, 12U);

	Macro_Set_Bit(TIM3->EGR, 0U);
	Macro_Clear_Bit(TIM3->SR, 0U);

	Macro_Set_Bit(TIM3->CR1, 7U);
	Macro_Set_Bit(TIM3->CR1, 0U);
}

void TIM3_PWM_Set_Pulse(unsigned char ch, unsigned short pulse_us)
{
	switch (ch)
	{
	case 0: TIM3->CCR1 = pulse_us; break;
	case 1: TIM3->CCR2 = pulse_us; break;
	case 2: TIM3->CCR3 = pulse_us; break;
	case 3: TIM3->CCR4 = pulse_us; break;
	default: break;
	}
}