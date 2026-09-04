#include "device_driver.h"
#include "recycle.h"
#include "servo4.h"

extern volatile unsigned long g_sys_tick;

// 서보 채널 배정: 상단 투입구 게이트 1개 + 하단 분류 모터 2개
// (분류 모터가 1개가 아니라 2개인 이유: 아래 Set_Sort_Position 참고 - 두 모터의 조합으로 4가지 배출 방향을 만들어냄)
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

// ms 단위로 바쁜 대기(busy-wait)하는 지연 함수. g_sys_tick은 timer.c가
// 1ms마다 1씩 올려주는 전역 카운터이므로, 그 차이가 ms에 도달할 때까지 대기.
static void Delay_ms(unsigned long ms)
{
    unsigned long start = g_sys_tick;
    while ((g_sys_tick - start) < ms);
}

// 서보 3개(게이트+분류모터2개)를 초기 위치(게이트 닫힘, 분류모터 중립)로 세팅
void Recycle_Init(void)
{
    Servo4_Init(); // 서보 3개(CH0~CH2) 전부 90도로 1차 초기화

    Servo4_Set_Angle(GATE_CH, GATE_CLOSE_ANGLE);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);
}

// 분류 종류별 하단 모터 2개의 목표 각도 (PET/CAN/PAPER/OTHER -> 4방향)
// SORT1, SORT2 두 모터를 각각 LEFT/RIGHT로 조합하면 2x2=4가지 경우가 나오고,
// 그 4가지를 각각 PET/CAN/PAPER/OTHER 네 가지 분류 결과에 대응시킨 것
// (마치 2비트로 4가지 상태를 표현하는 것과 같은 방식)
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
    case RECYCLE_VINYL:
        Servo4_Set_Angle(SORT1_CH, SORT_RIGHT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_RIGHT_ANGLE);
        break;
    default:
        Servo4_Set_Angle(SORT1_CH, SORT_RIGHT_ANGLE);
        Servo4_Set_Angle(SORT2_CH, SORT_RIGHT_ANGLE);
        break;
    }
}

// 분류 결과 하나를 실제로 처리: 방향 잡기 -> 투입구 열기 -> 닫고 복귀,
// 순서대로 진행하며 각 단계 사이에 서보/쓰레기가 움직일 시간을 Delay_ms로 확보.
void Recycle_Process(RecycleType type)
{
    if (type == RECYCLE_NONE)
    {
        return; // 분류 결과 없음 -> 아무 동작 안 함
    }

    //하단 분류 모터를 목표 방향으로 먼저 이동
    Set_Sort_Position(type);
    Delay_ms(300); // 서보가 목표 각도까지 움직일 시간 확보

    //상단 투입구 게이트 열어서 쓰레기 투입
    Servo4_Set_Angle(GATE_CH, GATE_OPEN_ANGLE);
    Delay_ms(500); // 쓰레기 떨어질 시간

    //게이트 닫고, 하단 모터 중앙(대기 위치)으로 복귀
    Servo4_Set_Angle(GATE_CH, GATE_CLOSE_ANGLE);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);
}
