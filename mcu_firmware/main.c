// RTOS 없이 super loop 구조: 매 반복마다 (1) UART 명령 처리 (2) 서보 램프 진행
// (3) 주기적 도어/적재량 보고를 블로킹 없이 번갈아 확인
// 적재율 계산은 bin_filter.c(블랭킹->중앙값->비대칭EMA 3단계 필터)에 위임한다.
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

// 4개 통 raw 거리를 읽어 bin_filter로 3단계 필터링한 뒤, 최종 적재율(%)만 Jetson에 보고
static void Report_Bin_Fill(void)
{
    float raw_paper = Ultra_Read_cm(BIN_ULTRA_CH[0]);
    float raw_can   = Ultra_Read_cm(BIN_ULTRA_CH[1]);
    float raw_pet   = Ultra_Read_cm(BIN_ULTRA_CH[2]);
    float raw_vinyl = Ultra_Read_cm(BIN_ULTRA_CH[3]);

    BinFilter_Update(BIN_PAPER, raw_paper, g_sys_tick);
    BinFilter_Update(BIN_CAN,   raw_can,   g_sys_tick);
    BinFilter_Update(BIN_PET,   raw_pet,   g_sys_tick);
    BinFilter_Update(BIN_VINYL, raw_vinyl, g_sys_tick);

    int p_paper = BinFilter_Get_Percent(BIN_PAPER);
    int p_can   = BinFilter_Get_Percent(BIN_CAN);
    int p_pet   = BinFilter_Get_Percent(BIN_PET);
    int p_vinyl = BinFilter_Get_Percent(BIN_VINYL);

    printf("$BIN:%d/%d/%d/%d  (raw cm: %.1f/%.1f/%.1f/%.1f)\n",
           p_paper, p_can, p_pet, p_vinyl, raw_paper, raw_can, raw_pet, raw_vinyl);
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

        int bin_idx = Recycle_Type_To_Bin_Index(type);
        if (bin_idx >= 0)
        {
            BinFilter_Notify_Drop((BinType)bin_idx, g_sys_tick);
        }

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
    unsigned long last_tick = 0L;

    Sys_Init(115200);
    printf("\n=== Recycling Sorter (Servo + Ultrasonic + Jetson UART) ===\n");
    printf("Command format: <servo 1~3> <angle 0~180> [speed deg/s]  (e.g. \"1 90\" or \"1 90 30\")\n");
    printf("Jetson protocol: $DOOR_OPEN:<PET|CAN|PAPER|VINYL>  /  $DOOR_CLOSE\n");

    Timer_Init();
    Ultra_Init();
    Recycle_Init();
    BinFilter_Init();

    // 캘리브레이션: 센서가 통 입구 위 10cm에 장착, 통 깊이는 30cm
    // -> 빈 통(0%) = 10+30 = 40cm, 가득 참(100%) = 10cm
    // *** 이 4줄이 빠지면 기본값(30cm/5cm)으로 계산돼서 값이 이상하게 나옵니다 ***
    BinFilter_Config_Distance(BIN_PAPER, 40.0f, 10.0f);
    BinFilter_Config_Distance(BIN_CAN,   40.0f, 10.0f);
    BinFilter_Config_Distance(BIN_PET,   40.0f, 10.0f);
    BinFilter_Config_Distance(BIN_VINYL, 40.0f, 10.0f);

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