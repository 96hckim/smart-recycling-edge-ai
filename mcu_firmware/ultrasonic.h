#ifndef ULTRASONIC_H
#define ULTRASONIC_H

// CH0=PC2/3(기존 센서 하위호환), CH1=PC0/1, CH2=PC4/5, CH3=PC10/11 - 전부 GPIOC
typedef enum
{
    ULTRA_CH0 = 0,
    ULTRA_CH1 = 1,
    ULTRA_CH2 = 2,
    ULTRA_CH3 = 3,
    ULTRA_COUNT = 4
} Ultra_Ch;

void Ultra_Init(void);

// 실패 시 -1.0 반환 (타임아웃/범위초과/잘못된 채널 구분 없이 "측정 불가"로 통일)
float Ultra_Read_cm(Ultra_Ch ch);

#endif
