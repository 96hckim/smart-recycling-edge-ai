// RTOS 없이 super loop 구조: 매 반복마다 (1) UART 명령 처리 (2) 서보 램프 진행
// (3) 주기적 도어/적재량 보고를 블로킹 없이 번갈아 확인
// [디버깅용] bin_filter 계산 빼고 초음파 raw 거리(cm)를 그대로 출력
#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"
#include "servo.h"
#include "recycle.h"
#include "bin_filter.h"
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

// BinType(0=PAPER,1=CAN,2=PET,3=VINYL) 순서와 정확히 일치하는 초음파 채널 매핑
static const Ultra_Ch BIN_ULTRA_CH[BIN_COUNT] = { ULTRA_CH0, ULTRA_CH1, ULTRA_CH2, ULTRA_CH3 };

static void Report_Door_State(void)
{
    printf("$DOOR_STATE:%s\n", (Recycle_Get_Gate_State() == GATE_OPEN) ? "OPEN" : "CLOSED");
}

// [디버깅용] bin_filter 계산(중앙값/EMA/블랭킹) 빼고 측정된 raw 거리(cm)를 그대로 출력
static void Report_Bin_Fill(void)
{
    float d_paper = Ultra_Read_cm(BIN_ULTRA_CH[0]);
    float d_can   = Ultra_Read_cm(BIN_ULTRA_CH[1]);
    float d_pet   = Ultra_Read_cm(BIN_ULTRA_CH[2]);
    float d_vinyl = Ultra_Read_cm(BIN_ULTRA_CH[3]);

    printf("$RAW_CM: PAPER=%.1f CAN=%.1f PET=%.1f VINYL=%.1f\n", d_paper, d_can, d_pet, d_vinyl);
}

// Jetson 쪽에서 보내는 '$'로 시작하는 프로토콜 명령 처리 (분류 결과에 따른 도어 제어)
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

// 진입점: 초기화 순서 고정(클럭/UART -> 타이머/센서 -> 인터럽트 활성화) 후 super loop 진입
void Main(void)
{
    unsigned long last_tick = 0L;

    Sys_Init(115200);
    printf("\n=== Recycling Sorter [DEBUG: raw ultrasonic cm] ===\n");
    printf("Command format: <servo 1~3> <angle 0~180> [speed deg/s]  (e.g. \"1 90\" or \"1 90 30\")\n");
    printf("Jetson protocol: $DOOR_OPEN:<PET|CAN|PAPER|VINYL>  /  $DOOR_CLOSE\n");

    Timer_Init();
    Ultra_Init();
    Recycle_Init();
    // BinFilter_Init()/Config_Distance()는 이 디버깅 버전에서 안 씀 (raw 값만 볼 거라)

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

        if ((g_sys_tick - last_tick) >= 2000)
        {
            last_tick = g_sys_tick;

            Report_Bin_Fill();
        }
    }
}