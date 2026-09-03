#include "device_driver.h"

/**
 * @brief 시스템 클럭(SYSCLK)을 내부 HSI 기반 96MHz로 초기화
 *
 * [클럭 트리 구성]
 * - HSI (16MHz) / PLLM(8) * PLLN(192) / PLLP(4) = SYSCLK 96MHz
 *   계산: 16MHz / 8 = 2MHz(VCO 입력) -> 2MHz * 192 = 384MHz(VCO 출력) -> 384MHz / 4 = 96MHz
 * - AHB Prescaler = /1  -> HCLK  = 96MHz
 * - APB1 Prescaler = /2 -> PCLK1 = 48MHz (TIMx: 96MHz, option.h의 TIMXCLK 참고)
 * - APB2 Prescaler = /1 -> PCLK2 = 96MHz
 *
 * 이 함수 하나가 실행된 뒤부터 CPU와 각 버스(AHB/APB1/APB2)가 위 속도로 동작하고,
 * 이후 uart.c(PCLK1/PCLK2 기준 보레이트 계산), timer.c/ultrasonic.c(TIMXCLK 기준
 * 프리스케일러 계산)가 이 클럭값을 전제로 동작함.
 */
void Clock_Init(void)
{
    // 1. HSI(내부 16MHz RC 발진기) 클럭 활성화 및 안정화(Ready) 대기
    //    RCC->CR bit0 = HSION(HSI 켜기), bit1 = HSIRDY(안정화 완료 플래그, 읽기전용)
    RCC->CR |= (1 << 0);
    while (!Macro_Check_Bit_Set(RCC->CR, 1))
        ;

    // 2. Flash 레이턴시(3 Wait State @ 96MHz - 클럭이 빠를수록 플래시 읽기 지연을 더 줘야 함)
    //    및 프리페치/I-Cache/D-Cache 동시 활성화 (읽기 성능 향상)
    //    bit10=DCEN(D-Cache), bit9=ICEN(I-Cache), bit8=PRFTEN(Prefetch), bit[2:0]=LATENCY(3)
    FLASH->ACR = (1 << 10) | (1 << 9) | (1 << 8) | (0x3 << 0);

    // 3. PLL 설정 (PLLM=8, PLLN=192, PLLP=4(레지스터값 0b01→"×2"가 아니라 P=4를 뜻함), PLLSRC=HSI)
    //    RCC->PLLCFGR 비트 배치: [27:24]=PLLQ, [22]=PLLSRC(0=HSI,1=HSE), [17:16]=PLLP,
    //                            [14:6]=PLLN, [5:0]=PLLM
    RCC->PLLCFGR = (8 << 24) | (0 << 22) | (1 << 16) | (192 << 6) | (8 << 0);

    // 4. PLL 활성화(CR bit24=PLLON) 및 Lock(bit25=PLLRDY, 즉 출력 안정화) 대기
    Macro_Set_Bit(RCC->CR, 24);
    while (!Macro_Check_Bit_Set(RCC->CR, 25))
        ;

    // 5. APB1/APB2 분주비 설정 (PCLK1 = HCLK/2 = 48MHz, PCLK2 = HCLK/1 = 96MHz)
    //    RCC->CFGR 비트 배치: [15:13]=PPRE2(APB2), [12:10]=PPRE1(APB1), [7:4]=HPRE(AHB)
    //    PPRE1=4(0b100)=/2, PPRE2=0=/1, HPRE=0=/1
    RCC->CFGR = (0 << 13) | (4 << 10) | (0 << 4);

    // 6. 시스템 클럭 소스를 PLL로 전환(CFGR[1:0]=SW=0b10) 및 전환 완료(CFGR[3:2]=SWS==0b10) 대기
    Macro_Write_Block(RCC->CFGR, 0x3, 0x2, 0);
    while (Macro_Extract_Area(RCC->CFGR, 0x3, 2) != 0x2)
        ;
}
