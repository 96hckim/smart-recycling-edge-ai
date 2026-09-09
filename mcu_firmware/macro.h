// 레지스터 read-modify-write를 직접 쓰면 실수하기 쉬워서 비트 단위 헬퍼로 통일
#define Macro_Set_Bit(dest, pos) ((dest) |= ((unsigned)0x1 << (pos)))

#define Macro_Clear_Bit(dest, pos) ((dest) &= ~((unsigned)0x1 << (pos)))

#define Macro_Invert_Bit(dest, pos) ((dest) ^= ((unsigned)0x1 << (pos)))

#define Macro_Clear_Area(dest, bits, pos) ((dest) &= ~(((unsigned)bits) << (pos)))

#define Macro_Set_Area(dest, bits, pos) ((dest) |= (((unsigned)bits) << (pos)))

#define Macro_Invert_Area(dest, bits, pos) ((dest) ^= (((unsigned)bits) << (pos)))

// GPIO MODER/AFR, PWM CCR처럼 여러 비트로 표현되는 필드를 통째로 갈아끼울 때 사용
#define Macro_Write_Block(dest, bits, data, pos) ((dest) = (((unsigned)dest) & ~(((unsigned)bits) << (pos))) | (((unsigned)data) << (pos)))

#define Macro_Extract_Area(dest, bits, pos) ((((unsigned)dest) >> (pos)) & (bits))

#define Macro_Check_Bit_Set(dest, pos) ((((unsigned)dest) >> (pos)) & 0x1)

#define Macro_Check_Bit_Clear(dest, pos) (!((((unsigned)dest) >> (pos)) & 0x1))
