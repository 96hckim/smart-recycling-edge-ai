#include "device_driver.h"
#include "ultrasonic.h"
#include <stdio.h>
#define TRIG_PIN 2  // PC2
#define ECHO_PIN 3  // PC3

// TIM4는 16비트 타이머(0~65535)이므로 CNT 차이는 반드시 16비트 폭으로 계산해야
// 카운터가 0xFFFF -> 0으로 롤오버될 때도 경과 시간이 올바르게 계산됨
static inline unsigned short Tim4_Elapsed_us(unsigned short start)
{
    return (unsigned short)((unsigned short)TIM4->CNT - start);
}

static void Delay_us(unsigned int us)
{
    unsigned short start = (unsigned short)TIM4->CNT;
    while (Tim4_Elapsed_us(start) < us);
}

void Ultra_Init(void)
{
    // GPIOC 클럭
    Macro_Set_Bit(RCC->AHB1ENR, 2U);

    // PC2: 출력(Trig), Push-Pull
    Macro_Write_Block(GPIOC->MODER, 0x3, 0x1, TRIG_PIN * 2);
    Macro_Clear_Bit(GPIOC->OTYPER, TRIG_PIN);
    Macro_Clear_Bit(GPIOC->ODR, TRIG_PIN);

    // PC3: 입력(Echo), 풀다운
    Macro_Write_Block(GPIOC->MODER, 0x3, 0x0, ECHO_PIN * 2);
    Macro_Write_Block(GPIOC->PUPDR, 0x3, 0x2, ECHO_PIN * 2);

    // TIM4: 1us 프리런 카운터 (16비트, 0~65535us ≈ 65.5ms까지 측정 가능)
    // -> 기존 시스템 틱 용도에서 해제된 TIM4를 재활용 (systick.c 참고)
    Macro_Set_Bit(RCC->APB1ENR, 2U);
    TIM4->PSC = (unsigned int)(TIMXCLK / 1000000.0 + 0.5) - 1U; // 15
    TIM4->ARR = 0xFFFFU;
    Macro_Set_Bit(TIM4->EGR, 0U);
    Macro_Clear_Bit(TIM4->SR, 0U);
    Macro_Set_Bit(TIM4->CR1, 0U);
}

float Ultra_Read_cm(void)
{
    unsigned short t_start, pulse_us;

    // 1. Trig 신호 출력 (10us High Pulse)
    Macro_Clear_Bit(GPIOC->ODR, TRIG_PIN);
    Delay_us(2);
    Macro_Set_Bit(GPIOC->ODR, TRIG_PIN);
    Delay_us(10);
    Macro_Clear_Bit(GPIOC->ODR, TRIG_PIN);

    // 2. Echo가 HIGH가 될 때까지 대기 (타임아웃 10ms)
    t_start = (unsigned short)TIM4->CNT;
    while (!(GPIOC->IDR & (1U << ECHO_PIN)))
    {
        if (Tim4_Elapsed_us(t_start) > 10000U)
        {
            return -1.0f; // 타임아웃
        }
    }

    // 3. Echo가 HIGH로 변한 정확한 시점 기록
    t_start = (unsigned short)TIM4->CNT;

    // 4. Echo가 LOW로 떨어질 때까지 대기 (타임아웃 30ms, 약 5m 범위)
    while (GPIOC->IDR & (1U << ECHO_PIN))
    {
        if (Tim4_Elapsed_us(t_start) > 30000U)
        {
            return -1.0f; // 타임아웃
        }
    }

    // 5. Echo Pulse Width 계산
    pulse_us = Tim4_Elapsed_us(t_start);

    // 거리(cm) = pulse_us / 58.0f
    return (float)pulse_us / 58.0f;
}