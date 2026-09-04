// =====================================================================
// ultrasonic.h
// -----------------------------------------------------------------
// HC-SR04류 초음파 거리센서(Trig/Echo 2핀) 드라이버 인터페이스.
// 최대 4개 채널을 지원 (전부 GPIOC 핀, TIM4 프리런 카운터 하나를 공유해서
// "한 번에 하나씩 순차적으로" 측정함 - 동시측정은 아님).
// =====================================================================

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

typedef enum
{
    ULTRA_CH0 = 0, // Trig=PC2,  Echo=PC3  (기존 센서)
    ULTRA_CH1 = 1, // Trig=PC0,  Echo=PC1
    ULTRA_CH2 = 2, // Trig=PC4,  Echo=PC5
    ULTRA_CH3 = 3, // Trig=PC10, Echo=PC11
    ULTRA_COUNT = 4
} Ultra_Ch;

void Ultra_Init(void); // 4채널 전부 Trig(출력)/Echo(입력) 핀 + 시간 측정용 TIM4 초기화

/**
 * @brief 지정한 채널로 거리 측정 (cm)
 * @param ch ULTRA_CH0~ULTRA_CH3
 * @return 정상: 거리(cm), 타임아웃(응답없음/범위초과) 또는 잘못된 채널: -1.0
 */
float Ultra_Read_cm(Ultra_Ch ch);

#endif // ULTRASONIC_H