// =====================================================================
// macro.h
// -----------------------------------------------------------------
// STM32 레지스터는 32비트 정수 하나에 여러 설정 비트가 뒤섞여 있어서
// (예: RCC->AHB1ENR의 2번 비트 = GPIOC 클럭 켜기), 매번
//   RCC->AHB1ENR = RCC->AHB1ENR | (1 << 2);
// 처럼 쓰면 코드가 길고 실수하기 쉽습니다.
// 아래 매크로들은 "레지스터의 특정 비트(또는 여러 비트 묶음)를
// 켜기/끄기/뒤집기/읽기/한번에 덮어쓰기" 하는 걸 짧게 표현하기 위한
// 헬퍼입니다. 이 프로젝트의 거의 모든 .c 파일(clock.c, uart.c,
// timer.c, ultrasonic.c ...)에서 레지스터를 만질 때 이 매크로들을
// 사용하니, 먼저 이 파일부터 이해하고 넘어가면 나머지 코드 읽기가
// 훨씬 쉬워집니다.
// =====================================================================

// dest 레지스터의 pos번째 비트(1개)를 1로 만든다.
// 예) Macro_Set_Bit(RCC->AHB1ENR, 2)  ->  AHB1ENR의 bit2만 1로 set (나머지 비트는 그대로)
#define Macro_Set_Bit(dest, pos) ((dest) |= ((unsigned)0x1 << (pos)))

// dest 레지스터의 pos번째 비트(1개)를 0으로 만든다. (나머지 비트는 그대로)
#define Macro_Clear_Bit(dest, pos) ((dest) &= ~((unsigned)0x1 << (pos)))

// dest 레지스터의 pos번째 비트를 1<->0으로 반전시킨다.
#define Macro_Invert_Bit(dest, pos) ((dest) ^= ((unsigned)0x1 << (pos)))

// dest 레지스터에서 pos번째 비트부터 bits 폭만큼(여러 비트)을 전부 0으로 지운다.
// 예) bits=0x3, pos=4 -> bit4, bit5 두 개를 0으로.
#define Macro_Clear_Area(dest, bits, pos) ((dest) &= ~(((unsigned)bits) << (pos)))

// dest 레지스터에서 pos번째 비트부터 bits 폭만큼을 전부 1로 세운다.
// (주의: 이미 0이 아닌 비트가 있으면 OR라서 원치 않는 값이 섞일 수 있음 -> 보통
//  값을 정확히 지정하고 싶을 땐 Macro_Write_Block을 사용)
#define Macro_Set_Area(dest, bits, pos) ((dest) |= (((unsigned)bits) << (pos)))

// dest 레지스터에서 pos번째 비트부터 bits 폭만큼을 반전시킨다.
#define Macro_Invert_Area(dest, bits, pos) ((dest) ^= (((unsigned)bits) << (pos)))

// dest 레지스터의 pos번째 비트부터 bits 폭만큼을 "그 부분만" data 값으로 통째로
// 덮어쓴다 (먼저 그 영역을 0으로 지운 뒤 data를 채워 넣는 read-modify-write).
// 이 프로젝트에서 GPIO 모드(MODER), AF 선택(AFR), PWM 듀티(CCMR) 등
// "여러 비트로 표현되는 설정값"을 세팅할 때 가장 많이 쓰이는 매크로입니다.
// 예) Macro_Write_Block(GPIOC->MODER, 0x3, 0x2, 12)
//     -> MODER 레지스터의 bit12~13(폭 2비트, mask 0x3)을 0b10(=AF 모드)으로 설정
#define Macro_Write_Block(dest, bits, data, pos) ((dest) = (((unsigned)dest) & ~(((unsigned)bits) << (pos))) | (((unsigned)data) << (pos)))

// dest 레지스터의 pos번째 비트부터 bits 폭만큼을 "읽어서" 그 값만 뽑아낸다.
// (쓰기 없이 현재 설정값을 확인할 때 사용, 예: 현재 클럭 소스가 뭔지 읽기)
#define Macro_Extract_Area(dest, bits, pos) ((((unsigned)dest) >> (pos)) & (bits))

// dest 레지스터의 pos번째 비트가 1이면 1(참), 0이면 0(거짓)을 반환.
// 주로 "플래그가 설정될 때까지 대기"하는 while(!Macro_Check_Bit_Set(...)) 형태로 사용.
#define Macro_Check_Bit_Set(dest, pos) ((((unsigned)dest) >> (pos)) & 0x1)

// dest 레지스터의 pos번째 비트가 0이면 1(참), 1이면 0(거짓)을 반환. (위와 반대)
#define Macro_Check_Bit_Clear(dest, pos) (!((((unsigned)dest) >> (pos)) & 0x1))
