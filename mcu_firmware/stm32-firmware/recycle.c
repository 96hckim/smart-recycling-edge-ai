#include "device_driver.h"
#include "recycle.h"
#include "servo4.h"

extern volatile unsigned long g_sys_tick;

// 서보 채널 배정: 상단 투입구 게이트 1개 + 하단 분류 모터 2개
#define GATE_CH  SERVO4_CH0  // 상단 투입구 게이트 (PC6)
#define SORT1_CH SERVO4_CH1  // 하단 분류 모터 1   (PC7)
#define SORT2_CH SERVO4_CH2  // 하단 분류 모터 2   (PC8)

// 게이트 각도 (0도: 닫힘, 90도: 열림) - 실제 기구에 맞게 조정
#define GATE_CLOSE_ANGLE 0
#define GATE_OPEN_ANGLE  90

// 하단 분류 모터 각도: SORT1/SORT2 두 개 조합으로 4방향 결정 - 실제 기구에 맞게 조정
#define SORT_LEFT_ANGLE  0
#define SORT_MID_ANGLE   90   // 대기(중립) 위치
#define SORT_RIGHT_ANGLE 180

static void Delay_ms(unsigned long ms)
{
    unsigned long start = g_sys_tick;
    while ((g_sys_tick - start) < ms)
        ;
}

void Recycle_Init(void)
{
    Servo4_Init(); // 서보 3개(CH0~CH2) 전부 90도로 1차 초기화

    Servo4_Set_Angle(GATE_CH, GATE_CLOSE_ANGLE);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);
}

// 분류 종류별 하단 모터 2개의 목표 각도 (PET/CAN/PAPER/OTHER -> 4방향)
static void Set_Sort_Position(RecycleType type)
{
    switch (type)
    {
    case RECYCLE_PET:
        Servo4_Set_Angle(SORT1_CH, SORT_LEFT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_LEFT_ANGLE);
        break;
    case RECYCLE_CAN:
        Servo4_Set_Angle(SORT1_CH, SORT_LEFT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_RIGHT_ANGLE);
        break;
    case RECYCLE_PAPER:
        Servo4_Set_Angle(SORT1_CH, SORT_RIGHT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_LEFT_ANGLE);
        break;
    case RECYCLE_OTHER:
    default:
        Servo4_Set_Angle(SORT1_CH, SORT_RIGHT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_RIGHT_ANGLE);
        break;
    }
}

void Recycle_Process(RecycleType type)
{
    if (type == RECYCLE_NONE)
    {
        return; // 분류 결과 없음 -> 아무 동작 안 함
    }

    // 1) 하단 분류 모터를 목표 방향으로 먼저 이동
    Set_Sort_Position(type);
    Delay_ms(300); // 서보가 목표 각도까지 움직일 시간 확보

    // 2) 상단 투입구 게이트 열어서 쓰레기 투입
    Servo4_Set_Angle(GATE_CH, GATE_OPEN_ANGLE);
    Delay_ms(500); // 쓰레기 떨어질 시간

    // 3) 게이트 닫고, 하단 모터 중앙(대기 위치)으로 복귀
    Servo4_Set_Angle(GATE_CH, GATE_CLOSE_ANGLE);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);
}