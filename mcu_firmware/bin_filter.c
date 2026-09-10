/**
 * @file    bin_filter.c
 * @brief   초음파 센서 수거함 적재율 3단계 필터 구현부
 */

#include "bin_filter.h"

/* 4개 수거함 필터 인스턴스 (정적 할당으로 힙 메모리 파편화 원천 방지) */
static BinFilter s_bins[BIN_COUNT];

/* ---------------------------------------------------- */
/* 내부 헬퍼 함수                                       */
/* ---------------------------------------------------- */

/**
 * @brief  작은 크기(5개) 정렬에 가장 최적화된 삽입 정렬(Insertion Sort)
 */
static void Sort_Samples(float *arr, uint8_t size)
{
    for (uint8_t i = 1; i < size; i++)
    {
        float key = arr[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/**
 * @brief  거리를 백분율(0.0 ~ 100.0%)로 선형 변환 및 클램핑
 */
static float Convert_Distance_To_Percent(const BinFilter *bin, float dist_cm)
{
    if (dist_cm >= bin->empty_cm) return 0.0f;
    if (dist_cm <= bin->full_cm)  return 100.0f;

    float pct = (bin->empty_cm - dist_cm) / (bin->empty_cm - bin->full_cm) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

/* ---------------------------------------------------- */
/* 공개 API 구현                                        */
/* ---------------------------------------------------- */

void BinFilter_Init(void)
{
    for (int i = 0; i < BIN_COUNT; i++)
    {
        s_bins[i].empty_cm = DEFAULT_BIN_EMPTY_CM;
        s_bins[i].full_cm  = DEFAULT_BIN_FULL_CM;
        s_bins[i].blanking_until_tick = 0U;
        s_bins[i].sample_count = 0U;
        s_bins[i].buf_idx = 0U;
        s_bins[i].filtered_dist_cm = DEFAULT_BIN_EMPTY_CM;
        s_bins[i].filtered_percent = 0.0f;
        s_bins[i].is_initialized = false;

        for (int j = 0; j < MEDIAN_WINDOW_SIZE; j++)
        {
            s_bins[i].median_buf[j] = DEFAULT_BIN_EMPTY_CM;
        }
    }
}

void BinFilter_Config_Distance(BinType bin, float empty_cm, float full_cm)
{
    if ((int)bin < 0 || (int)bin >= BIN_COUNT) return;
    if (empty_cm <= full_cm) return; // 유효성 검사

    s_bins[bin].empty_cm = empty_cm;
    s_bins[bin].full_cm  = full_cm;
}

void BinFilter_Notify_Drop(BinType bin, uint32_t current_tick_ms)
{
    if ((int)bin < 0 || (int)bin >= BIN_COUNT) return;

    /* [노이즈 1단계] 투입 후 2초간 계측을 무시하도록 블랭킹 만료 틱 설정 */
    s_bins[bin].blanking_until_tick = current_tick_ms + BLANKING_DURATION_MS;
}

float BinFilter_Update(BinType bin_idx, float raw_dist_cm, uint32_t current_tick_ms)
{
    if ((int)bin_idx < 0 || (int)bin_idx >= BIN_COUNT) return 0.0f;

    BinFilter *bin = &s_bins[bin_idx];

    /* [노이즈 1단계] 투입 진행 중 블랭킹 검사: 낙하 중 센서 가림 현상 무시 */
    if (current_tick_ms < bin->blanking_until_tick)
    {
        return bin->filtered_percent; // 이전 상태 그대로 유지
    }

    /* [예외 처리] 센서 타임아웃(0cm 이하) 또는 비정상적인 거리 초과 시 이전 값 유지 */
    if (raw_dist_cm <= 0.0f || raw_dist_cm > (bin->empty_cm + 15.0f))
    {
        return bin->filtered_percent;
    }

    /* [노이즈 2단계] 5-샘플 링버퍼에 저장 */
    bin->median_buf[bin->buf_idx] = raw_dist_cm;
    bin->buf_idx = (bin->buf_idx + 1U) % MEDIAN_WINDOW_SIZE;
    if (bin->sample_count < MEDIAN_WINDOW_SIZE)
    {
        bin->sample_count++;
    }

    /* 버퍼 복사 후 정렬하여 중앙값(Median) 추출 (스파이크/난반사 제거) */
    float sort_tmp[MEDIAN_WINDOW_SIZE];
    for (uint8_t i = 0; i < bin->sample_count; i++)
    {
        sort_tmp[i] = bin->median_buf[i];
    }
    Sort_Samples(sort_tmp, bin->sample_count);

    float median_dist = sort_tmp[bin->sample_count / 2];
    float current_raw_pct = Convert_Distance_To_Percent(bin, median_dist);

    /* [노이즈 3단계] 비대칭 지수 이동 평균 (Asymmetric EMA) 필터 */
    if (!bin->is_initialized)
    {
        /* 최초 측정 시 지연 없이 즉시 반영 */
        bin->filtered_dist_cm = median_dist;
        bin->filtered_percent = current_raw_pct;
        bin->is_initialized = true;
    }
    else
    {
        float alpha;

        /*
         * 적재율이 상승할 때: 쓰레기가 쌓이는 정상 동작이므로 빠르게 추종 (alpha = 0.25)
         * 적재율이 하강할 때: 센서 난반사나 쓰레기 틈새 투과 가능성이 크므로 극도로 보수적으로 반응 (alpha = 0.02)
         */
        if (current_raw_pct >= bin->filtered_percent)
        {
            alpha = EMA_ALPHA_RISING;
        }
        else
        {
            alpha = EMA_ALPHA_FALLING;
        }

        bin->filtered_percent = (alpha * current_raw_pct) + ((1.0f - alpha) * bin->filtered_percent);
        bin->filtered_dist_cm = (alpha * median_dist)     + ((1.0f - alpha) * bin->filtered_dist_cm);
    }

    return bin->filtered_percent;
}

int BinFilter_Get_Percent(BinType bin_idx)
{
    if ((int)bin_idx < 0 || (int)bin_idx >= BIN_COUNT) return 0;

    /* 소수점 반올림 처리 (0.5f 이상 올림) */
    int pct = (int)(s_bins[bin_idx].filtered_percent + 0.5f);
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

float BinFilter_Get_Distance(BinType bin_idx)
{
    if ((int)bin_idx < 0 || (int)bin_idx >= BIN_COUNT) return DEFAULT_BIN_EMPTY_CM;
    return s_bins[bin_idx].filtered_dist_cm;
}
