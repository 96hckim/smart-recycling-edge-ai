#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"
#include "servo4.h"
#include <stdio.h>

extern volatile unsigned long g_sys_tick;

// UART2 RX 인터럽트(isr.c)에서 조립해주는 한 줄 명령 버퍼
#define RX_LINE_BUF_SIZE 32
extern volatile char g_rx_line[RX_LINE_BUF_SIZE];
extern volatile unsigned char g_rx_line_ready;

static void Sys_Init(int baud)
{
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();
    Uart2_Init(baud);
    setvbuf(stdout, NULL, _IONBF, 0);
}

// ComPortMaster 등에서 "<서보번호> <각도>\n" 형식으로 보낸 명령 처리
// 예: "1 90"  -> 서보1(CH0)을 90도로
//     "2 0"   -> 서보2(CH1)을 0도로
//     "3 180" -> 서보3(CH2)을 180도로
static void Handle_Servo_Command(const char *line)
{
    int servo_num = 0;
    int angle = 0;

    if (sscanf(line, "%d %d", &servo_num, &angle) != 2)
    {
        printf("Invalid command: \"%s\" (format: <servo 1~3> <angle 0~180>)\n", line);
        return;
    }

    if (servo_num < 1 || servo_num > 3)
    {
        printf("Invalid servo number: %d (1~3만 가능)\n", servo_num);
        return;
    }

    if (angle < 0 || angle > 180)
    {
        printf("Invalid angle: %d (0~180만 가능)\n", angle);
        return;
    }

    Servo4_Ch ch = (Servo4_Ch)(servo_num - 1); // 1~3 -> SERVO4_CH0~CH2
    Servo4_Set_Angle(ch, (unsigned char)angle);
    printf("Servo%d -> %d deg\n", servo_num, angle);
}

void Main(void)
{
    unsigned long last_tick = 0L;

    Sys_Init(115200);
    printf("\n=== Ultrasonic + Servo Control ===\n");
    printf("Command format: <servo 1~3> <angle 0~180>  (e.g. \"1 90\")\n");

    Timer_Init();      // SysTick(1ms) + TIM3 PWM(서보 4채널) 초기화
    Ultra_Init();      // 초음파(TIM4) 초기화
    Servo4_Init();     // 서보 3개(CH0~CH2) 초기 각도 90도로 세팅

    Uart2_RX_Interrupt_Enable(1); // ComPortMaster 명령 수신 인터럽트 활성화

    for (;;)
    {
        // 1) UART로 서보 제어 명령 한 줄이 들어왔는지 확인
        if (g_rx_line_ready)
        {
            Handle_Servo_Command((const char *)g_rx_line);
            g_rx_line_ready = 0; // 처리 끝났으니 다음 줄 받을 수 있게 해제
        }

        // 2) 200ms마다 초음파 거리 측정
        if ((g_sys_tick - last_tick) >= 1000)
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