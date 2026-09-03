// =====================================================================
// recycle.h
// -----------------------------------------------------------------
// (참고) 이 모듈은 servo4.c 위에서 "재활용품 분류기" 동작을 구현한
// 상위 로직인데, main.c의 실제 흐름에서는 아직 호출되지 않고 있음
// (main.c는 UART 명령으로 직접 Servo4_Set_Angle을 호출). 아마도
// Jetson 등 외부 비전 장치가 재질(PET/CAN/PAPER/OTHER)을 판별해서
// 그 결과를 넘겨주면, 이 Recycle_Process()가 서보들을 움직여
// 자동으로 분류/투입까지 하도록 만든 모듈로 보인다.
// =====================================================================

#ifndef RECYCLE_H
#define RECYCLE_H

// Jetson에서 받는 분류 결과
typedef enum
{
    RECYCLE_NONE = 0,  // 분류 결과 없음 / 아직 안 옴 -> 아무 동작 안 함
    RECYCLE_PET,
    RECYCLE_CAN,
    RECYCLE_PAPER,
    RECYCLE_VINYL
} RecycleType;

// 서보(모터) 3개 초기화: 상단 투입구 게이트 1개 + 하단 분류 모터 2개
void Recycle_Init(void);

// 분류 결과에 따라 하단 모터로 방향 잡고, 상단 게이트 열어서 투입 -> 복귀까지 한 번에 처리
// type == RECYCLE_NONE 이면 아무 동작도 하지 않고 바로 리턴
void Recycle_Process(RecycleType type);

#endif // RECYCLE_H
