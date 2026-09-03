#include "device_driver.h"
#include "servo4.h"
#include "timer.h"

// timer.c에 정의된 전역 1ms 카운터 (속도 램프 시간 계산에 사용)
extern volatile unsigned long g_sys_tick;

// 채널별로 마지막에 설정한(도달했거나 진행 중인) 각도를 기억해두는 배열 (Servo4_Get_Angle에서 조회용)
static unsigned char g_servo4_angle[SERVO4_COUNT] = {0, 0, 0, 0};

// --- 속도 제어(램프)용 상태 ---
// 서보 자체는 "각도"만 받고 "속도" 개념이 없으므로, 목표각도까지 한 번에 점프시키지 않고
// 이 변수들을 이용해 Servo4_Update()가 매 호출마다 조금씩(경과시간 x 속도) 움직여준다.
static float g_servo4_current_f[SERVO4_COUNT];      // 현재 진행 중인 각도 (부드러운 이동을 위해 float로 보관)
static unsigned char g_servo4_target[SERVO4_COUNT]; // 최종 목표 각도
static float g_servo4_speed_deg_per_ms[SERVO4_COUNT]; // 채널별 이동속도(도/ms). 0이면 "이 채널은 램프 중이 아님"
static unsigned long g_servo4_last_tick[SERVO4_COUNT]; // 채널별 마지막으로 Update한 시각(ms)

// 서보 4개 전체 초기화: PWM 하드웨어를 켜고, 모든 채널을 중앙값인 90도로 이동시킨다.
void Servo4_Init(void)
{
    TIM3_PWM4_Init();
    Servo4_Set_Angle(SERVO4_CH0, 90);
    Servo4_Set_Angle(SERVO4_CH1, 90);
    Servo4_Set_Angle(SERVO4_CH2, 90);
    Servo4_Set_Angle(SERVO4_CH3, 90);

    for (int ch = 0; ch < SERVO4_COUNT; ch++)
    {
        g_servo4_current_f[ch] = 90.0f;
        g_servo4_target[ch] = 90;
        g_servo4_speed_deg_per_ms[ch] = 0.0f; // 초기에는 램프 중인 채널 없음
    }
}

// ch 채널의 서보를 angle(0~180도)로 이동시킨다.
// 일반적인 아날로그 서보 규격: 0도=0.5ms(500us) 펄스, 180도=2.5ms(2500us) 펄스,
// 그 사이는 선형 비례. 즉 pulse_us = 500 + angle * (2000/180)
void Servo4_Set_Angle(Servo4_Ch ch, unsigned char angle)
{
    if (ch >= SERVO4_COUNT) return;   // 잘못된 채널 번호 방어
    if (angle > 180) angle = 180;      // 각도 범위(0~180) 초과 시 180으로 clamp

    g_servo4_angle[ch] = angle; // 현재 각도 기억 (Servo4_Get_Angle이 나중에 조회)
    unsigned short pulse_us = 500 + ((unsigned short)angle * 2000 / 180); // 각도 -> 펄스폭(us) 변환
    TIM3_PWM_Set_Pulse((unsigned char)ch, pulse_us); // 실제 PWM 듀티(CCR) 반영
}

// ch 채널에 마지막으로 설정했던 각도를 반환 (하드웨어를 다시 읽는 게 아니라 소프트웨어가 마지막에 지시한 값을 그대로 돌려주는 것)
unsigned char Servo4_Get_Angle(Servo4_Ch ch)
{
    if (ch >= SERVO4_COUNT) return 0;
    return g_servo4_angle[ch];
}

// ch 채널을 angle까지 deg_per_sec 속도로 서서히 이동시키도록 "목표만" 등록.
// 실제 이동은 Servo4_Update()가 매 루프마다 조금씩 처리함 (여기서 즉시 움직이지 않음).
void Servo4_Set_Angle_Speed(Servo4_Ch ch, unsigned char angle, unsigned short deg_per_sec)
{
    if (ch >= SERVO4_COUNT) return;
    if (angle > 180) angle = 180;

    g_servo4_target[ch] = angle;

    if (deg_per_sec == 0)
    {
        // 속도 0 = 기존과 동일하게 즉시 이동 (램프 없음)
        Servo4_Set_Angle(ch, angle);
        g_servo4_current_f[ch] = (float)angle;
        g_servo4_speed_deg_per_ms[ch] = 0.0f;
        return;
    }

    g_servo4_speed_deg_per_ms[ch] = (float)deg_per_sec / 1000.0f; // 초당 도 -> ms당 도
    g_servo4_last_tick[ch] = g_sys_tick;
}

// main 루프에서 매 반복 호출: 램프 중인(speed>0) 채널들을 목표각도 쪽으로 한 스텝 이동
void Servo4_Update(void)
{
    for (int ch = 0; ch < SERVO4_COUNT; ch++)
    {
        if (g_servo4_speed_deg_per_ms[ch] <= 0.0f) continue; // 이 채널은 램프 중이 아님(도착했거나 속도0 이동)

        unsigned long now = g_sys_tick;
        unsigned long elapsed_ms = now - g_servo4_last_tick[ch];
        if (elapsed_ms == 0) continue; // 아직 1ms도 안 지났으면 스킵 (불필요한 PWM 갱신 방지)
        g_servo4_last_tick[ch] = now;

        float target = (float)g_servo4_target[ch];
        float step = g_servo4_speed_deg_per_ms[ch] * (float)elapsed_ms; // 이번 구간 동안 움직일 각도

        if (g_servo4_current_f[ch] < target)
        {
            g_servo4_current_f[ch] += step;
            if (g_servo4_current_f[ch] >= target)
            {
                g_servo4_current_f[ch] = target;
                g_servo4_speed_deg_per_ms[ch] = 0.0f; // 도착 완료
            }
        }
        else
        {
            g_servo4_current_f[ch] -= step;
            if (g_servo4_current_f[ch] <= target)
            {
                g_servo4_current_f[ch] = target;
                g_servo4_speed_deg_per_ms[ch] = 0.0f; // 도착 완료
            }
        }

        unsigned char cur_angle = (unsigned char)(g_servo4_current_f[ch] + 0.5f); // 반올림
        g_servo4_angle[ch] = cur_angle;
        unsigned short pulse_us = 500 + ((unsigned short)cur_angle * 2000 / 180);
        TIM3_PWM_Set_Pulse((unsigned char)ch, pulse_us);
    }
}