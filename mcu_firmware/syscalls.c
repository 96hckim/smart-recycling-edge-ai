// OS 없는 MCU 환경에서 newlib이 요구하는 저수준 시스템 콜을 이 보드에 맞게 구현(retarget).
// printf/malloc이 여기 없으면 링크는 되지만 동작을 못 함.
#include "device_driver.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>

char *_sbrk(int inc)
{
    extern unsigned char __ZI_LIMIT__;
    static char *heap = (char *)0; // 호출 간 유지되는 힙 포인터

    char *prevHeap;
    char *nextHeap;

    if (heap == (char *)0)
        heap = (char *)HEAP_BASE;

    prevHeap = heap;
    nextHeap = (char *)((((unsigned int)heap + inc) + 0x7) & ~0x7); // 8바이트 정렬로 올림

    if ((unsigned int)nextHeap >= HEAP_LIMIT) // option.h의 4KB 힙 한도 초과 시 malloc이 NULL 받게 함
        return (char *)0;

    heap = nextHeap;
    return prevHeap;
}

// printf 내부에서 쓰는 저수준 송신이라 uart.c의 인터럽트 경로 대신 단순 폴링으로 구현
static void _Uart2_Send_Byte(char data)
{
    if (data == '\n')
    {
        while (!Macro_Check_Bit_Set(USART2->SR, 7))
            ;
        USART2->DR = '\r'; // "\r\n" 규약 맞추기
    }

    while (!Macro_Check_Bit_Set(USART2->SR, 7))
        ;
    USART2->DR = data;
}

// newlib printf()가 내부적으로 호출 - file 인자 무시하고 전부 USART2로 보냄
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        _Uart2_Send_Byte(*ptr++);
    }
    return len;
}

// _read의 저수준 수신 - 인터럽트 없이 데이터 들어올 때까지 블로킹
static int _Uart2_Get_Char(void)
{
    while (!Macro_Check_Bit_Set(USART2->SR, 5))
        ;
    return USART2->DR;
}

// scanf/getchar가 호출 - main.c는 isr.c 인터럽트 수신을 쓰므로 실제로는 거의 안 불림(예비 경로)
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
            _Uart2_Send_Byte(ch);
            *ptr++ = ch;
            count++;
        }
    }
    return count;
}

// 파일시스템/프로세스가 없는 환경이라 newlib이 링크 시 요구하는 나머지 콜은 최소 더미로 채움
int _lseek(int file, int ptr, int dir)
{
    return 0; // seek 개념 없음 - 항상 0
}

int _close(int file)
{
    return -1; // 닫을 파일 없음 - 항상 실패
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR; // UART를 문자 장치로 알려줌
    return 0;
}

int _isatty(int file)
{
    return 1; // printf 버퍼링 방식 결정에 쓰이므로 "터미널"로 응답
}

int _getpid(void)
{
    return 1; // 프로세스 개념 없음 - 고정 PID
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL; // 죽일 프로세스가 없으므로 항상 에러
    return -1;
}
