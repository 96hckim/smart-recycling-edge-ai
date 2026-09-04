// =====================================================================
// option.h
// -----------------------------------------------------------------
// 프로젝트 전체에서 공통으로 쓰는 "설정값(상수)"을 모아둔 파일.
// 클럭 속도, 메모리 맵/스택·힙 크기처럼 여러 파일(clock.c, uart.c,
// timer.c, syscalls.c ...)이 함께 참조해야 하는 값들을 여기 한 곳에
// 모아두면, 나중에 클럭이나 메모리 크기를 바꿀 때 이 파일만 고치면
// 됩니다.
// =====================================================================

#ifndef OPTION_H
#define OPTION_H

// ----------------------------------------------------
// 시스템 클럭 주파수 설정 (Hz)
// -> 실제 클럭 초기화 코드는 clock.c의 Clock_Init()에 있고,
//    여기 값들은 "Clock_Init()이 이렇게 설정해 놨다"는 걸 다른 파일들이
//    계산에 쓸 수 있도록 상수로 정의해둔 것 (실제로 레지스터를 건드리진 않음)
// ----------------------------------------------------
#define SYSCLK (96000000U)               // 시스템 클럭 96MHz (clock.c: HSI 16MHz -> PLL로 96MHz)
#define HCLK (SYSCLK)                     // AHB 클럭 = SYSCLK (분주 없음, /1)
#define PCLK2 (HCLK)                      // APB2 클럭 = HCLK (분주 없음, /1) -> USART1 등
#define PCLK1 (HCLK / 2U)                 // APB1 클럭 = HCLK/2 = 48MHz -> USART2 등
// 타이머(TIMx) 클럭 규칙: APB 분주비가 1배면 TIMxCLK = PCLKx,
// 분주비가 1보다 크면(여기선 APB1이 /2) TIMxCLK = PCLKx * 2 가 된다.
// (STM32 하드웨어 규칙) -> 그래서 APB1에 물린 TIM3/TIM4는 PCLK1(48MHz)이 아니라
// 실제로는 96MHz로 동작함 (timer.c, ultrasonic.c의 PSC 계산에 사용됨)
#define TIMXCLK ((HCLK == PCLK1) ? (PCLK1) : (PCLK1 * 2U))

// ----------------------------------------------------
// STM32F411 SRAM 메모리 맵 (128KB: 0x20000000 ~ 0x20020000)
// -> 링커 스크립트(rom_0x08000000.lds)가 잡아주는 RAM 시작/끝 주소와 맞춰서
//    아래에서 힙(heap)/스택(stack) 영역을 사람이 계산해 나눠주는 용도
// ----------------------------------------------------
#define RAM_START (0x20000000U)
#define RAM_END (0x20020000U)

// 힙 및 스택 영역 정의 (8-byte 정렬)
// __ZI_LIMIT__ : 링커 스크립트가 정의하는 심볼로, .bss(0으로 초기화되는 전역변수) 영역이
//                끝나는 주소. 그 바로 다음부터를 힙으로 쓴다.
#define HEAP_BASE (((unsigned int)&__ZI_LIMIT__ + 0x7) & ~0x7)  // 8바이트 정렬로 올림
#define HEAP_SIZE (4 * 1024U) // 4KB 힙 할당 (malloc 등이 이 범위 안에서만 동작, syscalls.c의 _sbrk 참고)
#define HEAP_LIMIT (HEAP_BASE + HEAP_SIZE)

#define STACK_LIMIT (HEAP_LIMIT + 8U) // 힙과 스택 사이에 8바이트 여유(가드) 확보
#define STACK_BASE (RAM_END + 1U)     // 스택은 SRAM 맨 끝에서 시작해 "아래로" 자라남
#define STACK_SIZE (STACK_BASE - STACK_LIMIT)

#endif // OPTION_H
