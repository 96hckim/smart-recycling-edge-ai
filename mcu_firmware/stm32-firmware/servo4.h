#ifndef SERVO4_H
#define SERVO4_H

typedef enum
{
    SERVO4_CH0 = 0, // PC6
    SERVO4_CH1 = 1, // PC7
    SERVO4_CH2 = 2, // PC8
    SERVO4_CH3 = 3, // PC9
    SERVO4_COUNT = 4
} Servo4_Ch;

void Servo4_Init(void);
void Servo4_Set_Angle(Servo4_Ch ch, unsigned char angle);
unsigned char Servo4_Get_Angle(Servo4_Ch ch);

#endif // SERVO4_H