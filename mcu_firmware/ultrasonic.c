// =====================================================================
// ultrasonic.c
// -----------------------------------------------------------------
// HC-SR04류 초음파 센서 동작 원리:
//   1) Trig 핀에 10us 이상 HIGH 펄스를 주면 센서가 초음파를 쏨
//   2) 센서는 초음파를 쏘고, 반사파(echo)가 돌아올 때까지 Echo 핀을 HIGH로 유지
//   3) Echo가 HIGH로 유지된 시간(us)을 재면, 그 시간에 비례해서 왕복 거리를 계산 가능
//      거리(cm) = (음속 약 340m/s로 계산했을 때) echo_us / 58 (관용적으로 쓰이는 상수)
// 시간 측정에는 SysTick 대신, us 단위로 자유롭게 흐르는(free-running) TIM4
// 카운터를 사용함 (SysTick은 1ms 단위라 너무 성기고, TIM4는 원래 시스템 틱
// 용도로 안 쓰여서 재활용 가능 - timer.c 주석 참고).
//
// 4채널 지원: TIM4 카운터 하나를 그대로 공유해서, 채널마다 정해진
// Trig/Echo 핀 쌍을 순서대로(한 번에 하나씩) Trig -> Echo 대기 -> 펄스폭 측정
// 하는 방식. 완전한 "동시측정"은 아니지만, 4개를 빠르게 돌아가며 재는 것만으로
// 분류기 용도로는 충분함 (완전 동시측정을 하려면 4개 EXTI 인터럽트로 재설계 필요).
// =====================================================================
#include "device_driver.h"
#include "ultrasonic.h"
#include <stdio.h>

// 채널별 Trig/Echo 핀 번호 (전부 GPIOC, 기존에 안 쓰던 핀들로 배정)
// CH0: 기존 센서 그대로 PC2/PC3 유지 (하위 호환)
static const unsigned char TRIG_PIN[ULTRA_COUNT] = {2, 0, 4, 10}; // PC2, PC0, PC4, PC10
static const unsigned char ECHO_PIN[ULTRA_COUNT] = {3, 1, 5, 11}; // PC3, PC1, PC5, PC11

// TIM4는 16비트 타이머(0~65535)이므로 CNT 차이는 반드시 16비트 폭으로 계산해야
// 카운터가 0xFFFF -> 0으로 롤오버될 때도 경과 시간이 올바르게 계산됨
// (예: start=65530, 현재=3 이어도 unsigned short 뺄셈이면 자동으로 9가 나옴)
static inline unsigned short Tim4_Elapsed_us(unsigned short start)
{
    return (unsigned short)((unsigned short)TIM4->CNT - start);
}

// us 단위로 바쁜 대기(busy-wait)하는 소프트웨어 지연 함수.
// TIM4가 1us마다 1씩 증가하도록 설정돼 있으므로(Ultra_Init 참고),
// 카운터가 us만큼 흐를 때까지 그냥 계속 확인만 하면서 기다림.
static void Delay_us(unsigned int us)
{
    unsigned short start = (unsigned short)TIM4->CNT;
    while (Tim4_Elapsed_us(start) < us);
}

// Trig(출력)/Echo(입력) 핀 GPIO 설정 4채널 전부 + 시간 측정용 TIM4 프리런 카운터 초기화
void Ultra_Init(void)
{
    // GPIOC 클럭
    Macro_Set_Bit(RCC->AHB1ENR, 2U);

    for (int ch = 0; ch < ULTRA_COUNT; ch++)
    {
        unsigned char trig = TRIG_PIN[ch];
        unsigned char echo = ECHO_PIN[ch];

        // Trig: 출력, Push-Pull, 초기값 LOW
        Macro_Write_Block(GPIOC->MODER, 0x3, 0x1, trig * 2); // MODER 0b01 = 범용 출력
        Macro_Clear_Bit(GPIOC->OTYPER, trig);                  // 0 = Push-Pull(기본 출력 방식)
        Macro_Clear_Bit(GPIOC->ODR, trig);                     // 시작은 LOW

        // Echo: 입력, 풀다운 (신호가 없을 때 LOW로 안정되게)
        Macro_Write_Block(GPIOC->MODER, 0x3, 0x0, echo * 2); // MODER 0b00 = 입력
        Macro_Write_Block(GPIOC->PUPDR, 0x3, 0x2, echo * 2); // PUPDR 0b10 = 풀다운
    }

    // TIM4: 1us 프리런 카운터 (16비트, 0~65535us ≈ 65.5ms까지 측정 가능)
    // -> 기존 시스템 틱 용도에서 해제된 TIM4를 재활용 (timer.c의 SysTick_1ms_Init 참고)
    // 4채널이 전부 이 카운터 하나를 공유해서 순서대로 측정함
    Macro_Set_Bit(RCC->APB1ENR, 2U); // TIM4 클럭
    TIM4->PSC = (unsigned int)(TIMXCLK / 1000000.0 + 0.5) - 1U; // 카운터 1틱 = 1us가 되도록 (계산값: 15)
    TIM4->ARR = 0xFFFFU;             // 최대값까지 채우고 자동으로 0부터 다시 (자유 실행)
    Macro_Set_Bit(TIM4->EGR, 0U);    // UG: 설정값 즉시 반영
    Macro_Clear_Bit(TIM4->SR, 0U);   // 업데이트 플래그 클리어
    Macro_Set_Bit(TIM4->CR1, 0U);    // CEN = 1, 카운터 시작
}

// 지정한 채널(ch)의 초음파 센서로 거리 1회 측정.
// 정상이면 cm 값을, 응답이 없거나 범위를 벗어나거나 잘못된 채널이면 -1.0을 반환
// (main.c에서 -1이면 "timeout"으로 출력)
float Ultra_Read_cm(Ultra_Ch ch)
{
    if (ch >= ULTRA_COUNT) return -1.0f; // 잘못된 채널 번호 방어

    unsigned char trig = TRIG_PIN[ch];
    unsigned char echo = ECHO_PIN[ch];
    unsigned short t_start, pulse_us;

    // 1. Trig 신호 출력 (10us High Pulse) - 먼저 확실히 LOW로 만들고 나서 HIGH를 줌
    Macro_Clear_Bit(GPIOC->ODR, trig);
    Delay_us(2);
    Macro_Set_Bit(GPIOC->ODR, trig);
    Delay_us(10);
    Macro_Clear_Bit(GPIOC->ODR, trig);

    // 2. Echo가 HIGH가 될 때까지 대기 (타임아웃 10ms: 센서가 아예 응답 안 하는 경우 대비)
    t_start = (unsigned short)TIM4->CNT;
    while (!(GPIOC->IDR & (1U << echo)))
    {
        if (Tim4_Elapsed_us(t_start) > 10000U)
        {
            return -1.0f; // 타임아웃
        }
    }

    // 3. Echo가 HIGH로 변한 정확한 시점 기록 (여기서부터 펄스 폭을 잴 시작점)
    t_start = (unsigned short)TIM4->CNT;

    // 4. Echo가 LOW로 떨어질 때까지 대기 (타임아웃 30ms, 약 5m 범위 - 그 이상이면 측정 포기)
    while (GPIOC->IDR & (1U << echo))
    {
        if (Tim4_Elapsed_us(t_start) > 30000U)
        {
            return -1.0f; // 타임아웃
        }
    }

    // 5. Echo Pulse Width 계산 (Echo가 HIGH로 유지된 시간 = 초음파 왕복 시간)
    pulse_us = Tim4_Elapsed_us(t_start);

    // 거리(cm) = pulse_us / 58.0f
    // (음속 약 340m/s 기준, 왕복 거리이므로 /2, 단위 환산까지 합쳐진 관용 상수 58)
    return (float)pulse_us / 58.0f;
}