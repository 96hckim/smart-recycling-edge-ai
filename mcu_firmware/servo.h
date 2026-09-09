// TIM3 PWM 3채널(PC6~PC8)의 서보를 "채널+각도"로 다루는 래퍼. 펄스 생성 자체는 timer.c 담당.
#ifndef SERVO_H
#define SERVO_H

typedef enum
{
    SERVO_CH0 = 0,
    SERVO_CH1 = 1,
    SERVO_CH2 = 2,
    SERVO_COUNT = 3
} Servo_Ch;

void Servo_Init(void);
void Servo_Set_Angle(Servo_Ch ch, unsigned char angle);
unsigned char Servo_Get_Angle(Servo_Ch ch);

// deg_per_sec만큼 서서히 이동 (목표만 등록, 실제 이동은 Servo_Update가 매 루프 진행)
void Servo_Set_Angle_Speed(Servo_Ch ch, unsigned char angle, unsigned short deg_per_sec);

void Servo_Update(void);

#endif
