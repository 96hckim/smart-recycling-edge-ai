// [모터 제어 전용] 초음파/bin_filter 없이 서보+젯슨 프로토콜만
#include "device_driver.h"
#include "timer.h"
#include "servo.h"
#include "recycle.h"
#include <stdio.h>
#include <string.h>

extern volatile unsigned long g_sys_tick;
extern volatile char g_rx_line[RX_LINE_BUF_SIZE];
extern volatile unsigned char g_rx_line_ready;

static void Sys_Init(int baud)
{
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();
    Uart2_Init(baud);
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void Handle_Servo_Command(const char *line)
{
    int servo_num = 0;
    int angle = 0;
    int speed = 0;

    int n = sscanf(line, "%d %d %d", &servo_num, &angle, &speed);
    if (n != 3 && n != 2)
    {
        printf("Invalid command: \"%s\" (format: <servo 1~3> <angle 0~180> [speed deg/s])\n", line);
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

    if (speed < 0)
    {
        printf("Invalid speed: %d (0 이상만 가능)\n", speed);
        return;
    }

    Servo_Ch ch = (Servo_Ch)(servo_num - 1);
    Servo_Set_Angle_Speed(ch, (unsigned char)angle, (unsigned short)speed);

    if (speed > 0)
        printf("Servo%d -> %d deg (speed %d deg/s)\n", servo_num, angle, speed);
    else
        printf("Servo%d -> %d deg (instant)\n", servo_num, angle);
}

static void Report_Door_State(void)
{
    printf("$DOOR_STATE:%s\n", (Recycle_Get_Gate_State() == GATE_OPEN) ? "OPEN" : "CLOSED");
}

static void Handle_Jetson_Command(const char *line)
{
    if (strncmp(line, "$DOOR_OPEN:", 11) == 0)
    {
        const char *type_str = line + 11;
        RecycleType type = Recycle_Type_From_String(type_str);

        if (type == RECYCLE_NONE)
        {
            printf("Invalid $DOOR_OPEN type: \"%s\" (PET/CAN/PAPER/VINYL만 가능)\n", type_str);
            return;
        }

        Recycle_Door_Open(type);
        printf("Door open -> %s\n", type_str);
    }
    else if (strcmp(line, "$DOOR_CLOSE") == 0)
    {
        Recycle_Door_Close_Request();
        printf("$DOOR_CLOSE received\n");
    }
    else
    {
        printf("Unknown command from Jetson: \"%s\"\n", line);
    }
}

void Main(void)
{
    Sys_Init(115200);
    printf("\n=== Motor Control Only (no ultrasonic) ===\n");
    printf("Command format: <servo 1~3> <angle 0~180> [speed deg/s]  (e.g. \"1 90\" or \"1 90 30\")\n");
    printf("Jetson protocol: $DOOR_OPEN:<PET|CAN|PAPER|VINYL>  /  $DOOR_CLOSE\n");

    Timer_Init();
    Recycle_Init();

    Uart2_RX_Interrupt_Enable(1);

    for (;;)
    {
        if (g_rx_line_ready)
        {
            if (g_rx_line[0] == '$')
                Handle_Jetson_Command((const char *)g_rx_line);
            else
                Handle_Servo_Command((const char *)g_rx_line);
            g_rx_line_ready = 0;
        }

        if (Recycle_Gate_State_Changed())
        {
            Report_Door_State();
        }

        Servo_Update();

        {
            int reason = Recycle_Auto_Close_Update();

            if (reason == 1)
                printf("Door close: DOOR_CLOSE command\n");
            else if (reason == 2)
                printf("Door close: max open timeout (10s)\n");
        }
    }
}