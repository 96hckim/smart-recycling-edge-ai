// =====================================================================
// servo4.h
// -----------------------------------------------------------------
// TIM3 PWM 4채널(PC6~PC9)에 물린 서보모터 4개를 "채널 번호 + 각도(0~180도)"로
// 다루기 위한 간단한 래퍼(wrapper) API. 실제 PWM 신호 자체는
// timer.c의 TIM3_PWM_Set_Pulse()가 만들고, 여기서는 "각도 -> 펄스폭(us)"
// 변환과 각 채널의 현재 각도를 기억하는 역할만 함.
// recycle.c(분류기)가 이 API 위에서 게이트/분류 모터를 제어함.
// =====================================================================

#ifndef SERVO4_H
#define SERVO4_H

typedef enum
{
    SERVO4_CH0 = 0, // PC6 (TIM3_CH1)
    SERVO4_CH1 = 1, // PC7 (TIM3_CH2)
    SERVO4_CH2 = 2, // PC8 (TIM3_CH3)
    SERVO4_CH3 = 3, // PC9 (TIM3_CH4)
    SERVO4_COUNT = 4
} Servo4_Ch;

void Servo4_Init(void);                                   // PWM 초기화 + 4채널 전부 90도(중앙)로 세팅
void Servo4_Set_Angle(Servo4_Ch ch, unsigned char angle);  // ch 채널을 angle(0~180)도로 "즉시" 이동 (속도 무시)
unsigned char Servo4_Get_Angle(Servo4_Ch ch);              // ch 채널에 마지막으로 설정한(도달했거나 진행 중인) 각도 조회

// ch 채널을 angle(0~180)도까지 deg_per_sec(초당 몇 도) 속도로 "서서히" 이동시킨다.
// deg_per_sec == 0 이면 기존 Servo4_Set_Angle과 동일하게 즉시 이동(램프 없음).
// 실제 이동은 여기서 일어나지 않고, 목표값만 등록됨 - 매 루프마다 Servo4_Update()를
// 호출해줘야 그 목표를 향해 조금씩 실제로 움직인다 (super loop 구조라 non-blocking).
void Servo4_Set_Angle_Speed(Servo4_Ch ch, unsigned char angle, unsigned short deg_per_sec);

// main 루프에서 매 반복 호출: 속도 지정된 채널들을 목표각도 방향으로 한 스텝씩 이동시킴.
// 블로킹(딜레이) 없이 즉시 리턴하므로 초음파 측정 등 다른 일과 같이 돌려도 됨.
void Servo4_Update(void);

#endif // SERVO4_H