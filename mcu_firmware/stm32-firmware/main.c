#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"
#include <stdio.h>

extern volatile unsigned long g_sys_tick;

static void Sys_Init(int baud)
{
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();
    Uart2_Init(baud);
    setvbuf(stdout, NULL, _IONBF, 0);
}

void Main(void)
{
    unsigned long last_tick = 0L;

    Sys_Init(115200);
    printf("\n=== Ultrasonic Distance Test ===\n");

    Timer_Init();
    Ultra_Init();

    for (;;)
    {
        if ((g_sys_tick - last_tick) >= 200) // 200ms마다 측정
        {
            last_tick = g_sys_tick;

            float dist = Ultra_Read_cm();
            if (dist < 0)
            {
                printf("timeout (no echo)\n");
            }
            else
            {
                printf("distance = %.1f cm\n", dist);
            }
        }
    }
}