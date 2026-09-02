#ifndef ULTRASONIC_H
#define ULTRASONIC_H

void Ultra_Init(void);

/**
 * @brief 거리 측정 (cm)
 * @return 정상: 거리(cm), 타임아웃(응답없음/범위초과): -1.0
 */
float Ultra_Read_cm(void);

#endif // ULTRASONIC_H