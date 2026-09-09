// TIM3 PWM 4채널(PC6~PC9)의 서보를 "채널+각도"로 다루는 래퍼. 펄스 생성 자체는 timer.c 담당.
#ifndef SERVO4_H
#define SERVO4_H

typedef enum
{
    SERVO4_CH0 = 0,
    SERVO4_CH1 = 1,
    SERVO4_CH2 = 2,
    SERVO4_CH3 = 3,
    SERVO4_COUNT = 4
} Servo4_Ch;

void Servo4_Init(void);
void Servo4_Set_Angle(Servo4_Ch ch, unsigned char angle);
unsigned char Servo4_Get_Angle(Servo4_Ch ch);

// deg_per_sec만큼 서서히 이동 (목표만 등록, 실제 이동은 Servo4_Update가 매 루프 진행)
void Servo4_Set_Angle_Speed(Servo4_Ch ch, unsigned char angle, unsigned short deg_per_sec);

void Servo4_Update(void);

#endif
