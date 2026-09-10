/**
 * @file    bin_filter.h
 * @brief   초음파 센서 수거함 적재율 필터링 모듈 (Cortex-M4 / STM32 최적화)
 * @details 동적 할당(malloc) 없이 정적 구조체만 사용하며,
 *          투입 블랭킹 -> 5-샘플 중앙값(Median) -> 비대칭 EMA 3단계 필터를 적용합니다.
 */
#ifndef BIN_FILTER_H
#define BIN_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------- */
/* 전역 설정 및 파라미터                                */
/* ---------------------------------------------------- */
#define BIN_COUNT                 4       /* 수거함 개수 (0:종이, 1:캔, 2:페트, 3:비닐) */
#define MEDIAN_WINDOW_SIZE        5       /* 중앙값 필터 샘플 개수 (홀수 고정) */

#define DEFAULT_BIN_EMPTY_CM      30.0f   /* 빈 통 바닥까지의 실측 거리 (적재율 0%) */
#define DEFAULT_BIN_FULL_CM       5.0f    /* 만석 센서 앞 실측 거리 (적재율 100%) */

#define BLANKING_DURATION_MS      2000U   /* 투입 직후 계측 무시 시간 (2.0초) */

/* 비대칭 EMA 가중치 (Alpha) */
#define EMA_ALPHA_RISING          0.25f   /* 적재율 상승 시 (빠른 추종) */
#define EMA_ALPHA_FALLING         0.02f   /* 적재율 하강 시 (노이즈/틈새 튐 방지, 매우 느린 반영) */

/* ---------------------------------------------------- */
/* 열거형 및 데이터 구조체                              */
/* ---------------------------------------------------- */
typedef enum
{
    BIN_PAPER = 0,
    BIN_CAN   = 1,
    BIN_PET   = 2,
    BIN_VINYL = 3
} BinType;

typedef struct
{
    float empty_cm;                       /* 캘리브레이션: 빈 통 기준 거리 */
    float full_cm;                        /* 캘리브레이션: 가득 찬 통 기준 거리 */

    /* 1단계: 투입 블랭킹(낙하 시 튐 방지) 타이머 */
    uint32_t blanking_until_tick;

    /* 2단계: 5-샘플 중앙값 필터 링 버퍼 */
    float median_buf[MEDIAN_WINDOW_SIZE];
    uint8_t sample_count;                 /* 초기 버퍼 채움 카운터 */
    uint8_t buf_idx;                      /* 링 버퍼 삽입 인덱스 */

    /* 3단계: 비대칭 EMA 필터 상태 */
    float filtered_dist_cm;               /* 필터링된 최종 거리 (cm) */
    float filtered_percent;               /* 최종 필터링된 적재율 (0.0% ~ 100.0%) */
    bool is_initialized;                  /* 첫 유효 샘플 수신 여부 */
} BinFilter;

/* ---------------------------------------------------- */
/* 외부 노출 함수 (API)                                 */
/* ---------------------------------------------------- */

/**
 * @brief  전체 4개 수거함 필터 상태 초기화 (부팅 시 1회 호출)
 */
void BinFilter_Init(void);

/**
 * @brief  수거함별 빈 통/만석 물리 실측 거리 개별 보정 (선택 사항)
 */
void BinFilter_Config_Distance(BinType bin, float empty_cm, float full_cm);

/**
 * @brief  물체 투입 시작 알림 (투입 직후 2초간 초음파 계측 무시)
 * @param  bin 수거함 종류
 * @param  current_tick_ms 현재 시스템 틱 (ms, e.g. g_sys_tick)
 */
void BinFilter_Notify_Drop(BinType bin, uint32_t current_tick_ms);

/**
 * @brief  초음파 raw 거리(cm) 입력 및 3단계 노이즈 필터링 갱신
 * @param  bin 수거함 종류
 * @param  raw_dist_cm 센서 raw 계측 거리 (cm)
 * @param  current_tick_ms 현재 시스템 틱 (ms)
 * @return 현재 필터링된 적재율 (0.0f ~ 100.0f)
 */
float BinFilter_Update(BinType bin, float raw_dist_cm, uint32_t current_tick_ms);

/**
 * @brief  최종 정수 적재율(0 ~ 100%) 반환 (Jetson 통신 보고용)
 */
int BinFilter_Get_Percent(BinType bin);

/**
 * @brief  현재 필터링된 유효 거리(cm) 반환
 */
float BinFilter_Get_Distance(BinType bin);

#ifdef __cplusplus
}
#endif

#endif /* BIN_FILTER_H */
