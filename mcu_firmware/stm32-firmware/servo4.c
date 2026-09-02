#include "device_driver.h"
#include "servo4.h"
#include "timer.h"

static unsigned char g_servo4_angle[SERVO4_COUNT] = {0, 0, 0, 0};

void Servo4_Init(void)
{
    TIM3_PWM4_Init();
    Servo4_Set_Angle(SERVO4_CH0, 90);
    Servo4_Set_Angle(SERVO4_CH1, 90);
    Servo4_Set_Angle(SERVO4_CH2, 90);
    Servo4_Set_Angle(SERVO4_CH3, 90);
}

void Servo4_Set_Angle(Servo4_Ch ch, unsigned char angle)
{
    if (ch >= SERVO4_COUNT) return;
    if (angle > 180) angle = 180;

    g_servo4_angle[ch] = angle;
    unsigned short pulse_us = 500 + ((unsigned short)angle * 2000 / 180);
    TIM3_PWM_Set_Pulse((unsigned char)ch, pulse_us);
}

unsigned char Servo4_Get_Angle(Servo4_Ch ch)
{
    if (ch >= SERVO4_COUNT) return 0;
    return g_servo4_angle[ch];
}