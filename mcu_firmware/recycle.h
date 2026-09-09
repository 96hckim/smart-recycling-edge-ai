// servo4.c 위에 얹은 재활용 게이트 제어: Jetson이 넘긴 재질에 맞춰 분류 서보를 돌리고
// 게이트를 열며, 이후 자동 닫힘(타임아웃/비움 감지)까지 담당한다.
#ifndef RECYCLE_H
#define RECYCLE_H

typedef enum
{
    RECYCLE_NONE = 0,  // 아직 분류 결과 없음 / 알 수 없는 문자열
    RECYCLE_PET,
    RECYCLE_CAN,
    RECYCLE_PAPER,
    RECYCLE_VINYL
} RecycleType;

typedef enum
{
    GATE_CLOSED = 0,
    GATE_OPEN
} GateState;

void Recycle_Init(void);

RecycleType Recycle_Type_From_String(const char *s);

void Recycle_Door_Open(RecycleType type);

void Recycle_Door_Close(void);

GateState Recycle_Get_Gate_State(void);

int Recycle_Gate_State_Changed(void);

// main 루프에서 주기적으로 호출: 0=유지, 1=비움 감지로 닫음, 2=최대개방 타임아웃으로 닫음
int Recycle_Auto_Close_Update(float dist_cm);

RecycleType Recycle_Get_Open_Type(void);

#endif
