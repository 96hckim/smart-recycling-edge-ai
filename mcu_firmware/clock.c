#include "device_driver.h"

// HSI(16MHz) 기반으로 SYSCLK 96MHz까지 끌어올림 - 외부 크리스탈 없이도
// 동작해야 해서 HSE 대신 HSI를 PLL 소스로 선택.
// 이후 uart.c 보레이트, timer.c/ultrasonic.c 프리스케일러 계산이 전부
// 이 클럭 트리(HCLK 96 / PCLK1 48 / PCLK2 96)를 전제로 함.
void Clock_Init(void)
{

    RCC->CR |= (1 << 0);
    while (!Macro_Check_Bit_Set(RCC->CR, 1))
        ;

    // 96MHz 구동 시 플래시 읽기가 클럭을 못 따라가므로 3 wait state 필요
    FLASH->ACR = (1 << 10) | (1 << 9) | (1 << 8) | (0x3 << 0);

    // M=8, N=192, P=4 -> 16MHz/8=2MHz(VCO) *192=384MHz /4=96MHz
    RCC->PLLCFGR = (8 << 24) | (0 << 22) | (1 << 16) | (192 << 6) | (8 << 0);

    Macro_Set_Bit(RCC->CR, 24);
    while (!Macro_Check_Bit_Set(RCC->CR, 25))
        ;

    // APB1 타이머 클럭 한도(42MHz)를 넘지 않도록 PCLK1만 /2 (나머지는 /1)
    RCC->CFGR = (0 << 13) | (4 << 10) | (0 << 4);

    Macro_Write_Block(RCC->CFGR, 0x3, 0x2, 0);
    while (Macro_Extract_Area(RCC->CFGR, 0x3, 2) != 0x2)
        ;
}
