// RTOS 없이 super loop 구조: 매 반복마다 (1) UART 명령 처리 (2) 서보 램프 진행
// (3) 주기적 도어/적재량 보고를 블로킹 없이 번갈아 확인
#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"
#include "servo4.h"
#include "recycle.h"
#include <stdio.h>
#include <string.h>

extern volatile unsigned long g_sys_tick;

extern volatile char g_rx_line[RX_LINE_BUF_SIZE];
extern volatile unsigned char g_rx_line_ready;

// 부팅 직후 한 번만 필요한 설정(FPU/클럭/UART)을 모음 - 주변장치별 Init과 분리
static void Sys_Init(int baud)
{
    // CP10/CP11(FPU) Full Access - Ultra_Read_cm 등 float 연산을 쓰려면 필수
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();
    Uart2_Init(baud);
    // unbuffered: printf가 줄 끊김 없이 UART로 즉시 나가야 명령 응답이 안 밀림
    setvbuf(stdout, NULL, _IONBF, 0);

}

// 사람이 ComPortMaster 등으로 직접 서보를 테스트할 때 쓰는 디버그 명령 경로
// (Jetson 프로토콜인 $DOOR_OPEN 등과는 별개 - Main()에서 '$' 여부로 갈라짐)
static void Handle_Servo_Command(const char *line)
{
    int servo_num = 0;
    int angle = 0;
    int speed = 0; // 0 = 즉시 이동(속도 미지정 시 기존 동작과 호환)

    int n = sscanf(line, "%d %d %d", &servo_num, &angle, &speed);
    if (n != 3 && n != 2)
    {
        printf("Invalid command: \"%s\" (format: <servo 1~3> <angle 0~180> [speed deg/s])\n", line);
        return;
    }

    // 1~3만 받는 이유: CH0~CH2가 실제 게이트/분류 모터에 배선돼 있음(servo4.h)
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

    Servo4_Ch ch = (Servo4_Ch)(servo_num - 1);
    Servo4_Set_Angle_Speed(ch, (unsigned char)angle, (unsigned short)speed);

    if (speed > 0)
        printf("Servo%d -> %d deg (speed %d deg/s)\n", servo_num, angle, speed);
    else
        printf("Servo%d -> %d deg (instant)\n", servo_num, angle);
}

static const Ultra_Ch BIN_ULTRA_CH[4] = { ULTRA_CH0, ULTRA_CH1, ULTRA_CH2, ULTRA_CH3 };

// RecycleType 열거 순서와 초음파 채널 배열 순서가 별개라 둘을 이어주는 매핑이 필요
static int Recycle_Type_To_Bin_Index(RecycleType type)
{
    switch (type)
    {
    case RECYCLE_PAPER: return 0;
    case RECYCLE_CAN:   return 1;
    case RECYCLE_PET:   return 2;
    case RECYCLE_VINYL: return 3;
    default:            return -1;
    }
}

// 초음파-바닥 거리 기준 캘리브레이션 값(빈 통/꽉 찬 통) - 통 내부 실측으로 잡은 값
#define BIN_EMPTY_CM 30.0f
#define BIN_FULL_CM   5.0f

// 센서 raw 거리(cm)를 Jetson/로그가 바로 쓸 수 있는 적재율(%)로 변환
static int Distance_To_Fill_Percent(float dist_cm)
{
    if (dist_cm < 0)
    {
        return 0;
    }

    float pct = (BIN_EMPTY_CM - dist_cm) / (BIN_EMPTY_CM - BIN_FULL_CM) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return (int)(pct + 0.5f);
}

// 상태 변화 시점 + 주기적 호출 양쪽에서 다 쓰여서 Jetson이 놓치는 전이가 없게 함
static void Report_Door_State(void)
{
    printf("$DOOR_STATE:%s\n", (Recycle_Get_Gate_State() == GATE_OPEN) ? "OPEN" : "CLOSED");
}

// 4개 통 적재율을 한 줄 고정 포맷으로 묶어 보고 - Jetson 쪽 파싱을 단순하게 유지
static void Report_Bin_Fill(void)
{
    int fill[4];
    for (int i = 0; i < 4; i++)
    {
        float dist = Ultra_Read_cm(BIN_ULTRA_CH[i]);
        fill[i] = Distance_To_Fill_Percent(dist);
    }
    printf("$BIN:%d/%d/%d/%d\n", fill[0], fill[1], fill[2], fill[3]);
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
        // 닫힘은 항상 보드가 자동으로 판단(Recycle_Auto_Close_Update)하므로 수동 명령은 무시
        printf("$DOOR_CLOSE received (ignored - auto-close mode)\n");
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
    unsigned long last_auto_close_tick = 0L;

    Sys_Init(115200);
    printf("\n=== Ultrasonic + Servo Control ===\n");
    printf("Command format: <servo 1~3> <angle 0~180> [speed deg/s]  (e.g. \"1 90\" or \"1 90 30\")\n");

    Timer_Init();
    Ultra_Init();
    Recycle_Init();

    Uart2_RX_Interrupt_Enable(1);

    for (;;)
    {

        if (g_rx_line_ready)
        {
            // '$' 접두사로 Jetson 프로토콜과 사람의 서보 테스트 명령을 구분
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

        Servo4_Update();

        // 열린 문이 있을 때만 그 통의 거리를 재서 자동 닫힘 조건을 체크(불필요한 측정 방지)
        if ((g_sys_tick - last_auto_close_tick) >= 150)
        {
            last_auto_close_tick = g_sys_tick;

            int bin_idx = Recycle_Type_To_Bin_Index(Recycle_Get_Open_Type());
            if (bin_idx >= 0)
            {
                float dist = Ultra_Read_cm(BIN_ULTRA_CH[bin_idx]);
                int reason = Recycle_Auto_Close_Update(dist);

                if (reason == 1)
                    printf("Auto door close: empty debounce (dist=%.1fcm)\n", dist);
                else if (reason == 2)
                    printf("Auto door close: max open timeout (10s)\n");
            }
        }

        if ((g_sys_tick - last_tick) >= 1000)
        {
            last_tick = g_sys_tick;

            Report_Door_State();
            Report_Bin_Fill();
        }
    }
}
