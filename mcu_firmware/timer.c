#include "device_driver.h"
#include "timer.h"

// 1ms마다 SysTick_Handler(isr.c)에서 증가하는 전역 밀리초 카운터.
// main.c의 200ms 주기 측정, recycle.c의 Delay_ms() 등이 이 값을 기준으로 시간을 잰다.
volatile unsigned long g_sys_tick = 0;

// 이 프로젝트에서 쓰는 타이머 두 개를 한 번에 초기화하는 진입점 함수 (main.c에서 호출)
void Timer_Init(void)
{
	SysTick_1ms_Init();
	TIM3_PWM4_Init();
}

// 1ms 주기 시스템 틱: STM32 범용 타이머(TIM4) 대신 Cortex-M4 내장 SysTick 코어 타이머 사용
// -> TIM4는 초음파 센서(ultrasonic.c)의 1us 프리런 카운터로 재사용
void SysTick_1ms_Init(void)
{
	// SysTick_Config()는 CMSIS 표준 함수: 인자로 준 값(=클럭 주파수 / 1000, 즉 1ms에
	// 해당하는 카운트 수)만큼마다 SysTick 인터럽트(isr.c의 SysTick_Handler)가 발생하도록 설정
	SysTick_Config(SYSCLK / 1000U); // HCLK 기준 1ms마다 인터럽트
}

// 서보 4개용 50Hz PWM 초기화 (TIM3 CH1~CH4 = PC6, PC7, PC8, PC9)
// 서보모터는 보통 "20ms(50Hz) 주기마다 0.5~2.5ms 폭의 펄스"를 받아서
// 그 펄스 폭에 비례한 각도로 회전하는 구조. 이 함수는 그 PWM 신호를
// 만들어내기 위한 GPIO + 타이머 설정을 담당.
void TIM3_PWM4_Init(void)
{
	//GPIOC 포트에 클럭 공급 (클럭이 꺼져있으면 레지스터를 만져도 반응 없음)
	Macro_Set_Bit(RCC->AHB1ENR, 2U); // GPIOC 클럭

	//PC6~PC9를 "대체 기능(Alternate Function)" 모드로 설정
	//    MODER: 핀 2개 비트씩 사용, 0b10 = AF 모드
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 12U); // PC6 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 14U); // PC7 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 16U); // PC8 AF
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 18U); // PC9 AF

	//각 핀이 "여러 AF 기능 중 어떤 것"을 쓸지 AFR 레지스터에서 지정 (핀 4비트씩)
	//    AF2 = TIM3 채널로 연결 (데이터시트의 Alternate Function 매핑표 기준)
	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 24U); // PC6 -> AF2 (TIM3_CH1)
	Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 28U); // PC7 -> AF2 (TIM3_CH2)
	Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, 0U);  // PC8 -> AF2 (TIM3_CH3)
	Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, 4U);  // PC9 -> AF2 (TIM3_CH4)

	//TIM3에 클럭 공급 (APB1 버스)
	Macro_Set_Bit(RCC->APB1ENR, 1U); // TIM3 클럭

	//타이머 카운트 주기 설정
	//    PSC(프리스케일러): 타이머 입력 클럭(TIMXCLK)을 나눠서 카운터가 1씩 증가하는
	//    간격을 1us로 맞춘다. ARR(자동 재로딩 값): 카운터가 0~ARR까지 세고 다시 0으로
	//    돌아가므로, ARR=19999 -> 20000us(=20ms=50Hz) 주기가 됨 (서보 PWM 표준 주기)
	TIM3->PSC = (unsigned int)(TIMXCLK / 1000000.0 + 0.5) - 1U; // 카운터 1틱 = 1us가 되도록
	TIM3->ARR = 20000U - 1U;                                     // 20ms(50Hz) 주기

	//CH1~CH4를 "PWM 모드 1"로 설정 + 프리로드(OCxPE) 활성화
	//    CCMR1/CCMR2 레지스터에서 채널마다 [OCxM: 3비트][OCxPE: 1비트]를 세팅
	//    PWM 모드1(0b110)이면: 카운터 < CCR값일 때 출력 HIGH, 그 이후 LOW
	//    -> CCR(다음 함수에서 세팅)이 곧 "HIGH로 유지되는 펄스 폭(us)"이 됨
	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 4U);  // CH1: OC1M = 0b110 (PWM mode 1)
	Macro_Set_Bit(TIM3->CCMR1, 3U);                // CH1: OC1PE = 1 (프리로드 활성화)
	Macro_Write_Block(TIM3->CCMR1, 0x7, 0x6, 12U); // CH2: OC2M = 0b110
	Macro_Set_Bit(TIM3->CCMR1, 11U);               // CH2: OC2PE = 1
	Macro_Write_Block(TIM3->CCMR2, 0x7, 0x6, 4U);  // CH3: OC3M = 0b110
	Macro_Set_Bit(TIM3->CCMR2, 3U);                // CH3: OC3PE = 1
	Macro_Write_Block(TIM3->CCMR2, 0x7, 0x6, 12U); // CH4: OC4M = 0b110
	Macro_Set_Bit(TIM3->CCMR2, 11U);               // CH4: OC4PE = 1

	//CH1~CH4 출력을 실제 핀으로 내보내도록 활성화 (CCER의 CCxE 비트)
	Macro_Set_Bit(TIM3->CCER, 0U);  // CC1E
	Macro_Set_Bit(TIM3->CCER, 4U);  // CC2E
	Macro_Set_Bit(TIM3->CCER, 8U);  // CC3E
	Macro_Set_Bit(TIM3->CCER, 12U); // CC4E

	//위에서 건드린 PSC/ARR/CCMR 설정값을 실제 레지스터(섀도우 레지스터)에
	//    즉시 반영시키기 위해 업데이트 이벤트(UG)를 강제로 한 번 발생시키고,
	//    그로 인해 같이 세워지는 업데이트 인터럽트 플래그(UIF)는 바로 클리어
	Macro_Set_Bit(TIM3->EGR, 0U);   // UG: 강제 업데이트 이벤트 발생
	Macro_Clear_Bit(TIM3->SR, 0U);  // UIF 플래그 클리어

	//ARPE(자동 재로드 프리로드) 활성화 후 카운터 시작(CEN)
	Macro_Set_Bit(TIM3->CR1, 7U); // ARPE = 1
	Macro_Set_Bit(TIM3->CR1, 0U); // CEN = 1 (카운터 동작 시작)
}

// TIM3의 4개 채널(ch: 0~3) 중 하나의 PWM 펄스 폭(pulse_us, 단위 us)을 바꾼다.
// PSC 설정으로 카운터 1틱 = 1us이므로, CCRx 값을 그대로 "몇 us 동안 HIGH를
// 유지할지"로 쓸 수 있음 (servo4.c의 Servo4_Set_Angle()이 각도->펄스 변환 후 호출)
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
