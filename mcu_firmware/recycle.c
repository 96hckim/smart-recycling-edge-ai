#include "device_driver.h"
#include "recycle.h"
#include "servo4.h"
#include <string.h>

// Jetson이 판별한 재질(PET/CAN/PAPER/VINYL)을 받아 게이트+분류 서보를 구동하는 상위 로직.
extern volatile unsigned long g_sys_tick;

// 분류 방향이 4가지인데 모터는 2개뿐 -> Set_Sort_Position에서 LEFT/RIGHT 조합으로 4방향 표현
#define GATE_CH  SERVO4_CH0
#define SORT1_CH SERVO4_CH1
#define SORT2_CH SERVO4_CH2

#define GATE_CLOSE_ANGLE 0
#define GATE_OPEN_ANGLE  90

#define SORT_LEFT_ANGLE  0
#define SORT_MID_ANGLE   90
#define SORT_RIGHT_ANGLE 180

// timer.c가 1ms마다 올려주는 g_sys_tick 차이를 재는 busy-wait (별도 딜레이 타이머 불필요)
static void Delay_ms(unsigned long ms)
{
    unsigned long start = g_sys_tick;
    while ((g_sys_tick - start) < ms)
        ;
}

static volatile GateState s_gate_state = GATE_CLOSED;
static volatile GateState s_gate_state_prev = GATE_CLOSED;

// MIN_OPEN: 열자마자 거리센서가 "비었음"으로 오판해 바로 닫히는 것 방지 (투입 시간 최소 보장)
// EMPTY_DEBOUNCE: 센서 노이즈로 순간적으로 비어 보이는 것과 실제 비움을 구분하기 위한 유예 시간
// MAX_OPEN: 사람이 안 던지고 방치해도 무한정 열려있지 않도록 하는 failsafe
#define DOOR_MIN_OPEN_MS       2000
#define DOOR_EMPTY_DEBOUNCE_MS 1200
#define DOOR_MAX_OPEN_MS       10000
#define DOOR_CLEAR_CM          25.0f

static RecycleType s_open_type = RECYCLE_NONE;
static unsigned long s_gate_open_tick = 0;
static unsigned long s_clear_since_tick = 0;

// 게이트 서보 이동과 상태값 갱신을 한곳에 묶어, 상태 변경 지점을 여기 하나로 고정
static void Set_Gate_State(GateState state)
{
    Servo4_Set_Angle(GATE_CH, (state == GATE_OPEN) ? GATE_OPEN_ANGLE : GATE_CLOSE_ANGLE);
    s_gate_state = state;
}

// 전원 인가 직후 게이트/분류 서보를 알려진 초기 위치로 맞춰 상태 불일치를 방지
void Recycle_Init(void)
{
    Servo4_Init();

    Set_Gate_State(GATE_CLOSED);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);

    s_gate_state_prev = s_gate_state;
}

// SORT1/SORT2를 LEFT/RIGHT로 조합 = 2비트로 4가지 재질 방향을 표현
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

// Jetson이 UART로 보내는 문자열 프로토콜과 내부 enum 사이의 경계 지점
RecycleType Recycle_Type_From_String(const char *s)
{
    if (strcmp(s, "PET") == 0)   return RECYCLE_PET;
    if (strcmp(s, "CAN") == 0)   return RECYCLE_CAN;
    if (strcmp(s, "PAPER") == 0) return RECYCLE_PAPER;
    if (strcmp(s, "VINYL") == 0) return RECYCLE_VINYL;
    return RECYCLE_NONE;
}

// 분류부터 개방까지 한 번에 처리: 쓰레기가 잘못된 방향으로 떨어지지 않도록
// 분류 모터가 자리를 잡을 시간(Delay_ms)을 준 뒤에야 게이트를 연다
void Recycle_Door_Open(RecycleType type)
{
    if (type == RECYCLE_NONE)
    {
        return;
    }

    Set_Sort_Position(type);
    Delay_ms(300);
    Set_Gate_State(GATE_OPEN);

    s_open_type = type;
    s_gate_open_tick = g_sys_tick;
    s_clear_since_tick = 0;
}

// 수동/자동 닫힘 경로가 모두 여기로 모이도록 해 상태 리셋 누락을 방지
void Recycle_Door_Close(void)
{
    Set_Gate_State(GATE_CLOSED);
    Servo4_Set_Angle(SORT1_CH, SORT_MID_ANGLE);
    Servo4_Set_Angle(SORT2_CH, SORT_MID_ANGLE);

    s_open_type = RECYCLE_NONE;
    s_clear_since_tick = 0;
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

// 반환값 0: 유지, 1: 비움 감지로 닫음, 2: 최대개방 타임아웃으로 닫음 (main.c가 로그 구분에 사용)
int Recycle_Auto_Close_Update(float dist_cm)
{
    if (s_gate_state != GATE_OPEN)
    {
        s_clear_since_tick = 0;
        return 0;
    }

    unsigned long elapsed_open = g_sys_tick - s_gate_open_tick;

    if (elapsed_open >= DOOR_MAX_OPEN_MS)
    {
        Recycle_Door_Close();
        return 2;
    }

    if (elapsed_open < DOOR_MIN_OPEN_MS)
    {
        s_clear_since_tick = 0;
        return 0;
    }

    if (dist_cm < 0.0f || dist_cm < DOOR_CLEAR_CM)
    {
        // 측정 실패(음수) 또는 아직 물체 감지 -> 비움 판정 취소하고 다시 기다림
        s_clear_since_tick = 0;
        return 0;
    }

    if (s_clear_since_tick == 0)
    {
        // 방금 처음 "비어 보임"을 감지한 시점 기록 -> 디바운스 타이머 시작
        s_clear_since_tick = g_sys_tick;
        return 0;
    }

    if ((g_sys_tick - s_clear_since_tick) >= DOOR_EMPTY_DEBOUNCE_MS)
    {
        Recycle_Door_Close();
        return 1;
    }

    return 0;
}

RecycleType Recycle_Get_Open_Type(void)
{
    return s_open_type;
}
