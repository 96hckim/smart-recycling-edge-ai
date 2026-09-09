#include "device_driver.h"
#include "servo.h"
#include "timer.h"

extern volatile unsigned long g_sys_tick;

static unsigned char g_servo_angle[SERVO_COUNT] = {0, 0, 0};

// 서보는 "각도"만 받고 속도 개념이 없어서, 목표까지 즉시 점프하지 않고
// Servo_Update()가 매 호출마다 경과시간x속도만큼 current_f를 목표로 밀어주는 방식으로 램프를 흉내낸다.
static float g_servo_current_f[SERVO_COUNT];
static unsigned char g_servo_target[SERVO_COUNT];
static float g_servo_speed_deg_per_ms[SERVO_COUNT]; // 0이면 이 채널은 램프 중이 아님
static unsigned long g_servo_last_tick[SERVO_COUNT];

// PWM 하드웨어 켜기 + 3채널 전부를 중앙값(90도)으로 맞춰 기구가 어중간한 위치에서 시작하지 않게 함
void Servo_Init(void)
{
    TIM3_PWM4_Init();
    Servo_Set_Angle(SERVO_CH0, 90);
    Servo_Set_Angle(SERVO_CH1, 90);
    Servo_Set_Angle(SERVO_CH2, 90);

    for (int ch = 0; ch < SERVO_COUNT; ch++)
    {
        g_servo_current_f[ch] = 90.0f;
        g_servo_target[ch] = 90;
        g_servo_speed_deg_per_ms[ch] = 0.0f;
    }
}

// 즉시 이동 경로: 램프(속도 제어) 없이 바로 목표 펄스를 내보내는 최하위 진입점
void Servo_Set_Angle(Servo_Ch ch, unsigned char angle)
{
    if (ch >= SERVO_COUNT) return;
    if (angle > 180) angle = 180;

    g_servo_angle[ch] = angle;
    // 표준 아날로그 서보 규격(0도=500us, 180도=2500us)에 맞춘 선형 변환
    unsigned short pulse_us = 500 + ((unsigned short)angle * 2000 / 180);
    TIM3_PWM_Set_Pulse((unsigned char)ch, pulse_us);
}

unsigned char Servo_Get_Angle(Servo_Ch ch)
{
    if (ch >= SERVO_COUNT) return 0;
    return g_servo_angle[ch];
}

// 목표만 등록하고 즉시 리턴 - 실제 이동은 Servo_Update가 다음 루프부터 나눠서 진행
void Servo_Set_Angle_Speed(Servo_Ch ch, unsigned char angle, unsigned short deg_per_sec)
{
    if (ch >= SERVO_COUNT) return;
    if (angle > 180) angle = 180;

    g_servo_target[ch] = angle;

    if (deg_per_sec == 0)
    {
        // 속도 미지정 = 기존 즉시 이동 그대로 (램프 불필요)
        Servo_Set_Angle(ch, angle);
        g_servo_current_f[ch] = (float)angle;
        g_servo_speed_deg_per_ms[ch] = 0.0f;
        return;
    }

    g_servo_speed_deg_per_ms[ch] = (float)deg_per_sec / 1000.0f;
    g_servo_last_tick[ch] = g_sys_tick;
}

// non-blocking: main 루프에서 매 반복 호출해야 램프가 실제로 진행된다 (super loop 구조)
void Servo_Update(void)
{
    for (int ch = 0; ch < SERVO_COUNT; ch++)
    {
        if (g_servo_speed_deg_per_ms[ch] <= 0.0f) continue;

        unsigned long now = g_sys_tick;
        unsigned long elapsed_ms = now - g_servo_last_tick[ch];
        if (elapsed_ms == 0) continue;
        g_servo_last_tick[ch] = now;

        float target = (float)g_servo_target[ch];
        float step = g_servo_speed_deg_per_ms[ch] * (float)elapsed_ms;

        if (g_servo_current_f[ch] < target)
        {
            g_servo_current_f[ch] += step;
            if (g_servo_current_f[ch] >= target)
            {
                g_servo_current_f[ch] = target;
                g_servo_speed_deg_per_ms[ch] = 0.0f;
            }
        }
        else
        {
            g_servo_current_f[ch] -= step;
            if (g_servo_current_f[ch] <= target)
            {
                g_servo_current_f[ch] = target;
                g_servo_speed_deg_per_ms[ch] = 0.0f;
            }
        }

        unsigned char cur_angle = (unsigned char)(g_servo_current_f[ch] + 0.5f);
        g_servo_angle[ch] = cur_angle;
        unsigned short pulse_us = 500 + ((unsigned short)cur_angle * 2000 / 180);
        TIM3_PWM_Set_Pulse((unsigned char)ch, pulse_us);
    }
}
