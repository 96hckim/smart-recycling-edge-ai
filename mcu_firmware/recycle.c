#include "device_driver.h"
#include "recycle.h"
#include "servo.h"
#include <string.h>

// Jetson이 판별한 재질(PET/CAN/PAPER/VINYL)을 받아 3단 분류 트리(모터 3개)를 구동하는 상위 로직.
// TOP(투입구)이 먼저 PET/CAN 그룹 vs PAPER/VINYL 그룹으로 가르고,
// 그 아래 LEFT/RIGHT 모터가 각자 맡은 그룹을 다시 2개로 갈라 최종 4분류를 완성한다.
extern volatile unsigned long g_sys_tick;

#define TOP_CH   SERVO_CH0  // 투입구: LEFT->PET/CAN 그룹, RIGHT->PAPER/VINYL 그룹
#define LEFT_CH  SERVO_CH1  // PET/CAN 그룹 세부분류: LEFT->PET, RIGHT->CAN
#define RIGHT_CH SERVO_CH2  // PAPER/VINYL 그룹 세부분류: LEFT->PAPER, RIGHT->VINYL

#define ANGLE_LEFT  0
#define ANGLE_MID   90   // 중립/대기 각도 - 어느 쪽으로도 열려있지 않은 상태
#define ANGLE_RIGHT 180

static volatile GateState s_gate_state = GATE_CLOSED;
static volatile GateState s_gate_state_prev = GATE_CLOSED;

// MIN_OPEN: Jetson이 DOOR_CLOSE를 너무 일찍 보내도 최소 이 시간까지는 경로를 유지 (투입 시간 보장)
// MAX_OPEN: Jetson이 DOOR_CLOSE를 못 보내는 상황(오탐/통신 유실) 대비 failsafe
#define DOOR_MIN_OPEN_MS 2000
#define DOOR_MAX_OPEN_MS 10000

static RecycleType s_open_type = RECYCLE_NONE;
static unsigned long s_gate_open_tick = 0;
static volatile unsigned char s_close_requested = 0;

// 품목별 3모터 목표각 - 해당 없는 쪽 모터는 중립을 유지해 경로를 막아둔다
static void Set_Route(RecycleType type)
{
    switch (type)
    {
    case RECYCLE_PET:
        Servo_Set_Angle(TOP_CH, ANGLE_LEFT);
        Servo_Set_Angle(LEFT_CH, ANGLE_LEFT);
        Servo_Set_Angle(RIGHT_CH, ANGLE_MID);
        break;
    case RECYCLE_CAN:
        Servo_Set_Angle(TOP_CH, ANGLE_LEFT);
        Servo_Set_Angle(LEFT_CH, ANGLE_RIGHT);
        Servo_Set_Angle(RIGHT_CH, ANGLE_MID);
        break;
    case RECYCLE_PAPER:
        Servo_Set_Angle(TOP_CH, ANGLE_RIGHT);
        Servo_Set_Angle(RIGHT_CH, ANGLE_LEFT);
        Servo_Set_Angle(LEFT_CH, ANGLE_MID);
        break;
    case RECYCLE_VINYL:
        Servo_Set_Angle(TOP_CH, ANGLE_RIGHT);
        Servo_Set_Angle(RIGHT_CH, ANGLE_RIGHT);
        Servo_Set_Angle(LEFT_CH, ANGLE_MID);
        break;
    default:
        break;
    }
}

static void Set_Neutral(void)
{
    Servo_Set_Angle(TOP_CH, ANGLE_MID);
    Servo_Set_Angle(LEFT_CH, ANGLE_MID);
    Servo_Set_Angle(RIGHT_CH, ANGLE_MID);
}

// 전원 인가 직후 3모터를 중립 위치로 맞춰 상태 불일치를 방지
void Recycle_Init(void)
{
    Servo_Init();

    Set_Neutral();
    s_gate_state = GATE_CLOSED;
    s_gate_state_prev = s_gate_state;
}

// Jetson이 UART로 보내는 문자열 프로토콜과 내부 enum 사이의 경계 지점
RecycleType Recycle_Type_From_String(const char *s)
{
    if (strcmp(s, "PET") == 0)   return RECYCLE_PET;
    if (strcmp(s, "CAN") == 0)   return RECYCLE_CAN;
    if (strcmp(s, "PAPER") == 0) return RECYCLE_PAPER;
    if (strcmp(s, "VINYL") == 0) return RECYCLE_VINYL;
    return RECYCLE_NONE;
}

// 품목에 맞는 경로로 3모터를 즉시 이동 - 물리적 게이트가 따로 없어 이 각도 자체가 "열림"
void Recycle_Door_Open(RecycleType type)
{
    if (type == RECYCLE_NONE)
    {
        return;
    }

    Set_Route(type);

    s_gate_state = GATE_OPEN;
    s_open_type = type;
    s_gate_open_tick = g_sys_tick;
    s_close_requested = 0;
}

static void Do_Close(void)
{
    Set_Neutral();
    s_gate_state = GATE_CLOSED;
    s_open_type = RECYCLE_NONE;
    s_close_requested = 0;
}

// Jetson이 $DOOR_CLOSE(카메라에서 물체 사라짐)를 보냈을 때 호출.
// 최소 개방시간을 못 채웠으면 바로 닫지 않고 플래그만 세워 Recycle_Auto_Close_Update가 나중에 닫는다.
void Recycle_Door_Close_Request(void)
{
    s_close_requested = 1;
}

GateState Recycle_Get_Gate_State(void)
{
    return s_gate_state;
}

// main 루프가 매번 상태를 출력하지 않고 "바뀐 순간"에만 리포트하도록 하는 엣지 감지
int Recycle_Gate_State_Changed(void)
{
    if (s_gate_state != s_gate_state_prev)
    {
        s_gate_state_prev = s_gate_state;
        return 1;
    }
    return 0;
}

// main 루프가 주기적으로 호출: 0=유지, 1=DOOR_CLOSE 명령으로 닫음, 2=최대개방 타임아웃으로 닫음
int Recycle_Auto_Close_Update(void)
{
    if (s_gate_state != GATE_OPEN)
    {
        return 0;
    }

    unsigned long elapsed_open = g_sys_tick - s_gate_open_tick;

    if (elapsed_open >= DOOR_MAX_OPEN_MS)
    {
        Do_Close();
        return 2;
    }

    if (s_close_requested && elapsed_open >= DOOR_MIN_OPEN_MS)
    {
        Do_Close();
        return 1;
    }

    return 0;
}

RecycleType Recycle_Get_Open_Type(void)
{
    return s_open_type;
}
