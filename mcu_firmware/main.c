// =====================================================================
// main.c
// -----------------------------------------------------------------
// 프로그램 전체 흐름:
//   1) Sys_Init()로 클럭/UART 등 기본 시스템을 켠다
//   2) Timer_Init/Ultra_Init/Servo4_Init으로 각 주변장치를 초기화한다
//   3) 무한루프(for(;;))를 돌면서
//      - PC(ComPortMaster)에서 "서보번호 각도" 형식으로 명령이 오면 서보를 움직이고
//      - 200ms(=SysTick 200번)마다 한 번씩 초음파로 거리를 재서 출력한다
// 즉 이 보드는 "PC 명령으로 서보를 조종"하면서, 동시에 "주기적으로 초음파
// 거리를 계속 보고"하는 두 가지 일을 한 루프 안에서 번갈아 처리하는 구조
// (RTOS 없이 super loop 방식으로 동작).
// =====================================================================
#include "device_driver.h"
#include "timer.h"
#include "ultrasonic.h"
#include "servo4.h"
#include <stdio.h>

// timer.c에 정의된 전역 1ms 카운터
extern volatile unsigned long g_sys_tick;

// UART2 RX 인터럽트(isr.c)에서 조립해주는 한 줄 명령 버퍼
#define RX_LINE_BUF_SIZE 32
extern volatile char g_rx_line[RX_LINE_BUF_SIZE];
extern volatile unsigned char g_rx_line_ready;

// 보드 초기 부팅 시 딱 한 번 실행되는 기본 시스템 초기화
static void Sys_Init(int baud)
{
    // Cortex-M4 FPU(부동소수점 연산 장치) 사용 활성화.
    // SCB->CPACR에서 CP10, CP11(코프로세서 10,11 = FPU) 접근 권한을 "Full Access"로 설정.
    // float 연산(Ultra_Read_cm의 float 거리 계산 등)을 쓰려면 이 설정이 반드시 필요함.
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();      // 시스템 클럭을 96MHz로 설정 (clock.c)
    Uart2_Init(baud);  // UART2를 지정한 통신속도(baud)로 초기화 (uart.c)
    setvbuf(stdout, NULL, _IONBF, 0); // printf 출력 버퍼링을 끔(unbuffered)
                                       // -> printf 호출 즉시 UART로 바로 나가도록(줄 끊김/지연 방지)
}

// ComPortMaster 등에서 "<서보번호> <각도> [속도]\n" 형식으로 보낸 명령 처리
// 예: "1 90"      -> 서보1(CH0)을 90도로 "즉시" 이동 (속도 생략 = 기존과 동일)
//     "2 0 30"    -> 서보2(CH1)을 0도까지 초당 30도 속도로 "서서히" 이동
//     "3 180 90"  -> 서보3(CH2)을 180도까지 초당 90도 속도로 이동
static void Handle_Servo_Command(const char *line)
{
    int servo_num = 0;
    int angle = 0;
    int speed = 0; // 초당 몇 도(deg/sec). 입력 안 하면 0 = 즉시 이동(기존 동작)

    // 먼저 "숫자 숫자 숫자"(속도 포함) 형식으로 시도해보고, 안 되면 "숫자 숫자"(속도 생략)로 재시도
    int n = sscanf(line, "%d %d %d", &servo_num, &angle, &speed);
    if (n != 3 && n != 2)
    {
        printf("Invalid command: \"%s\" (format: <servo 1~3> <angle 0~180> [speed deg/s])\n", line);
        return;
    }

    // 서보 번호는 1~3만 유효 (CH0~CH2, 즉 게이트/분류모터 자리 - servo4.h 참고)
    if (servo_num < 1 || servo_num > 3)
    {
        printf("Invalid servo number: %d (1~3만 가능)\n", servo_num);
        return;
    }

    // 각도는 0~180도 범위만 유효
    if (angle < 0 || angle > 180)
    {
        printf("Invalid angle: %d (0~180만 가능)\n", angle);
        return;
    }

    // 속도는 0 이상만 유효 (0 = 즉시 이동)
    if (speed < 0)
    {
        printf("Invalid speed: %d (0 이상만 가능)\n", speed);
        return;
    }

    Servo4_Ch ch = (Servo4_Ch)(servo_num - 1); // 사람이 입력한 1~3 -> 실제 열거값 SERVO4_CH0~CH2로 변환
    Servo4_Set_Angle_Speed(ch, (unsigned char)angle, (unsigned short)speed); // 목표(각도/속도) 등록 (실제 이동은 Servo4_Update가 매 루프마다 진행)

    if (speed > 0)
        printf("Servo%d -> %d deg (speed %d deg/s)\n", servo_num, angle, speed);
    else
        printf("Servo%d -> %d deg (instant)\n", servo_num, angle);
}

// 프로그램 진입점 (일반 C의 main()에 해당, 스타트업 코드(crt0.s)에서 호출됨)
void Main(void)
{
    unsigned long last_tick = 0L; // 마지막으로 거리를 측정했던 시각(g_sys_tick 기준, ms)

    Sys_Init(115200); // 통신 속도 115200bps로 UART 등 기본 시스템 초기화
    printf("\n=== Ultrasonic + Servo Control ===\n");
    printf("Command format: <servo 1~3> <angle 0~180> [speed deg/s]  (e.g. \"1 90\" or \"1 90 30\")\n");

    Timer_Init();      // SysTick(1ms) + TIM3 PWM(서보 4채널) 초기화
    Ultra_Init();      // 초음파(TIM4) 초기화
    Servo4_Init();     // 서보 3개(CH0~CH2) 초기 각도 90도로 세팅

    Uart2_RX_Interrupt_Enable(1); // ComPortMaster 명령 수신 인터럽트 활성화 (isr.c의 USART2_IRQHandler가 이제부터 동작)

    // 메인 루프: 두 가지 일을 매 반복마다 "확인만 하고 바쁘게 대기하지 않는" 방식으로 번갈아 처리
    for (;;)
    {
        // 1) UART로 서보 제어 명령 한 줄이 들어왔는지 확인
        //    (g_rx_line_ready는 isr.c의 USART2_IRQHandler가 한 줄을 다 받으면 1로 세팅)
        if (g_rx_line_ready)
        {
            Handle_Servo_Command((const char *)g_rx_line);
            g_rx_line_ready = 0; // 처리 끝났으니 다음 줄 받을 수 있게 해제 (ISR이 다시 채울 수 있게)
        }

        // 속도 지정된(램프 중인) 서보 채널들을 목표각도 쪽으로 한 스텝 이동
        // (블로킹 딜레이 없이 즉시 리턴하므로 아래 초음파 측정과 같이 매 루프 돌아도 됨)
        Servo4_Update();

        // 2) 200ms마다 초음파 거리 측정 (4채널 순서대로 한 번씩)
        //    g_sys_tick은 1ms마다 1씩 증가하므로(isr.c의 SysTick_Handler),
        //    현재값 - 마지막 측정시각 >= 1000 이라고 되어있지만 실제로는
        //    1000ms(=1초)마다 측정하는 것과 같은 조건임 (주석과 실제 값(1000) 불일치 주의:
        //    진짜 200ms마다 재고 싶다면 아래 "1000"을 "200"으로 바꿔야 함)
        if ((g_sys_tick - last_tick) >= 1000)
        {
            last_tick = g_sys_tick; // 다음 비교를 위해 마지막 측정 시각 갱신

            for (int ch = 0; ch < ULTRA_COUNT; ch++)
            {
                float dist = Ultra_Read_cm((Ultra_Ch)ch); // 채널별로 순서대로 측정 (ultrasonic.c)
                if (dist < 0)
                {
                    printf("US%d: timeout (no echo)\n", ch); // 센서 응답 없음/범위 초과
                }
                else
                {
                    printf("US%d: distance = %.1f cm\n", ch, dist);
                }
            }
        }
    }
}