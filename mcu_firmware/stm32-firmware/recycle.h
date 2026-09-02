#ifndef RECYCLE_H
#define RECYCLE_H

// Jetson에서 받는 분류 결과
typedef enum
{
    RECYCLE_NONE = 0,  // 분류 결과 없음 / 아직 안 옴 -> 아무 동작 안 함
    RECYCLE_PET,
    RECYCLE_CAN,
    RECYCLE_PAPER,
    RECYCLE_OTHER
} RecycleType;

// 서보(모터) 3개 초기화: 상단 투입구 게이트 1개 + 하단 분류 모터 2개
void Recycle_Init(void);

// 분류 결과에 따라 하단 모터로 방향 잡고, 상단 게이트 열어서 투입 -> 복귀까지 한 번에 처리
// type == RECYCLE_NONE 이면 아무 동작도 하지 않고 바로 리턴
void Recycle_Process(RecycleType type);

#endif // RECYCLE_H