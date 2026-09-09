#ifndef OPTION_H
#define OPTION_H

#define SYSCLK (96000000U)
#define HCLK (SYSCLK)
#define PCLK2 (HCLK)
#define PCLK1 (HCLK / 2U)

// APB 분주가 걸리면(여기선 APB1 /2) 타이머 클럭은 PCLK가 아니라 2배로 뜀(STM32 하드웨어 규칙)
// -> TIM3/TIM4는 실제로 48이 아닌 96MHz로 동작 (timer.c/ultrasonic.c의 PSC 계산 기준)
#define TIMXCLK ((HCLK == PCLK1) ? (PCLK1) : (PCLK1 * 2U))

#define RAM_START (0x20000000U)
#define RAM_END (0x20020000U)

// __ZI_LIMIT__은 링커 스크립트가 정의하는 심볼(.bss 끝 주소) - 그 바로 뒤부터 힙으로 사용
#define HEAP_BASE (((unsigned int)&__ZI_LIMIT__ + 0x7) & ~0x7)
#define HEAP_SIZE (4 * 1024U)
#define HEAP_LIMIT (HEAP_BASE + HEAP_SIZE)

#define STACK_LIMIT (HEAP_LIMIT + 8U)
#define STACK_BASE (RAM_END + 1U)
#define STACK_SIZE (STACK_BASE - STACK_LIMIT)

#endif
