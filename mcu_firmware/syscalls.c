// =====================================================================
// syscalls.c
// -----------------------------------------------------------------
// newlib(표준 C 라이브러리)가 내부적으로 필요로 하는 저수준 시스템
// 호출들을 "이 임베디드 보드에 맞게" 직접 구현(retarget)한 파일.
// 일반 PC에서는 OS가 이런 기능들을 제공하지만, OS가 없는 마이크로컨트롤러
// 환경에서는 우리가 직접 채워줘야 printf/malloc 같은 표준 함수가 동작함.
// - _sbrk        : malloc()이 쓸 힙 메모리 공간을 늘려주는 함수
// - _write/_read : printf()/scanf() 등이 실제로 어디로 출력/입력할지
//                  (여기서는 UART2)를 연결해주는 함수
// - 나머지(_lseek, _close, _fstat, _isatty, _getpid, _kill)는 파일시스템/
//   프로세스가 없는 환경이라 최소한의 형식만 맞춰 응답하는 더미(stub) 구현
// =====================================================================
#include "device_driver.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>

/**
 * @brief 동적 메모리 할당(malloc)을 위한 힙(Heap) 관리 시스템 콜
 *
 * malloc()이 메모리를 더 요청하면(inc>0) 이 함수가 불려서, 현재 힙 포인터를
 * inc만큼 전진시키고 "전진하기 전" 주소를 돌려준다. option.h에서 정의한
 * HEAP_BASE(시작 주소)~HEAP_LIMIT(4KB 뒤, 끝 주소) 범위를 벗어나면 실패(NULL) 반환.
 */
char *_sbrk(int inc)
{
    extern unsigned char __ZI_LIMIT__; // 링커 스크립트가 정의: .bss 영역 끝 주소
    static char *heap = (char *)0;     // 현재 힙 포인터 (함수 호출 사이에도 값 유지)

    char *prevHeap;
    char *nextHeap;

    if (heap == (char *)0)
        heap = (char *)HEAP_BASE; // 첫 호출이면 힙 시작 주소로 초기화

    prevHeap = heap;
    nextHeap = (char *)((((unsigned int)heap + inc) + 0x7) & ~0x7); // 8바이트 정렬로 올림

    // 힙 영역 초과 방어 (4KB 힙을 다 쓰면 더 이상 할당 불가 -> malloc이 NULL 반환하게 됨)
    if ((unsigned int)nextHeap >= HEAP_LIMIT)
        return (char *)0;

    heap = nextHeap;
    return prevHeap; // 새로 확보된 영역의 시작 주소 반환
}

// UART2 폴링 방식 단일 바이트 송신 (인터럽트 없이, 보낼 수 있을 때까지 그냥 대기)
// uart.c의 Uart2_RX_Interrupt_Enable과 달리 여기는 "송신"이라 별도 인터럽트 없이
// 항상 폴링으로 처리 - printf 같은 표준함수 내부에서 쓰기 때문에 단순한 구현이 필요함
static void _Uart2_Send_Byte(char data)
{
    if (data == '\n')
    {
        while (!Macro_Check_Bit_Set(USART2->SR, 7)) // SR bit7 = TXE(송신 가능)
            ;
        USART2->DR = '\r'; // '\n' 앞에 '\r'을 붙여서 "\r\n"으로 (터미널 줄바꿈 규약)
    }

    while (!Macro_Check_Bit_Set(USART2->SR, 7))
        ;
    USART2->DR = data;
}

/**
 * @brief printf() 호출 시 UART2로 문자열 출력 리다이렉션
 *
 * newlib의 printf()는 내부적으로 이 _write()를 호출해서 실제 출력을 위임한다.
 * 여기서는 file 인자(어느 파일에 쓸지)는 무시하고, 무조건 UART2로 한 글자씩 보낸다.
 * -> 결과적으로 이 보드에서 printf("...")를 쓰면 그 내용이 USART2(PC와 연결된
 *    시리얼 포트)로 나가게 됨.
 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        _Uart2_Send_Byte(*ptr++);
    }
    return len;
}

// UART2 폴링 방식 단일 바이트 수신 (isr.c의 인터럽트 수신과는 별개의, 대기하며 읽는 경로)
static int _Uart2_Get_Char(void)
{
    while (!Macro_Check_Bit_Set(USART2->SR, 5)) // SR bit5 = RXNE(수신 데이터 있음)
        ;
    return USART2->DR;
}

/**
 * @brief scanf() / getchar() 호출 시 UART2로부터 문자열 입력 리다이렉션
 *
 * _write와 짝을 이루는 함수: newlib의 scanf/getchar가 이 _read()를 통해
 * 실제 입력을 받아온다. 한 줄(엔터까지) 또는 len 글자를 채울 때까지 반복해서
 * 읽고, 입력받은 글자를 그대로 다시 에코(echo) 출력해준다(터미널에서 타이핑이
 * 보이도록). 이 프로젝트의 main.c는 scanf 대신 isr.c의 인터럽트 수신 방식을
 * 쓰므로, 이 함수는 현재 실제로는 호출되지 않을 가능성이 높음(예비 코드).
 */
int _read(int file, char *ptr, int len)
{
    int count = 0;

    while (count < len)
    {
        char ch = _Uart2_Get_Char();

        if (ch == '\r' || ch == '\n')
        {
            _Uart2_Send_Byte('\r');
            _Uart2_Send_Byte('\n');
            *ptr++ = ch;
            count++;
            break;
        }
        else
        {
            _Uart2_Send_Byte(ch); // 입력한 글자를 그대로 에코
            *ptr++ = ch;
            count++;
        }
    }
    return count;
}

// 아래는 newlib이 컴파일/링크 시점에 존재를 요구하지만, 이 프로젝트에서는
// 실질적인 기능이 필요 없는 시스템 콜들의 최소 더미(stub) 구현.
// (파일시스템도, 여러 프로세스도 없는 단일 펌웨어 환경이라 대부분 "실패" 또는
//  "고정값"만 반환해도 표준 라이브러리 링크/동작에는 문제가 없음)

int _lseek(int file, int ptr, int dir)
{
    return 0; // 파일 탐색(seek) 기능 없음 -> 항상 0 반환
}

int _close(int file)
{
    return -1; // 닫을 파일이 없음 -> 실패 반환
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR; // "문자 장치(character device)"라고 알려줌 (UART이므로)
    return 0;
}

int _isatty(int file)
{
    return 1; // 항상 "터미널(대화형 장치)"이라고 응답 (버퍼링 방식 결정에 사용됨)
}

int _getpid(void)
{
    return 1; // 프로세스 개념이 없으므로 고정된 PID 1 반환
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL; // 프로세스를 죽이는 기능 자체가 없으므로 항상 에러
    return -1;
}
