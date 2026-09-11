/**
 * @file    bin_filter.c
 * @brief   초음파 센서 수거함 적재율 3단계 필터 구현부
 *          (블랭킹 -> 10-샘플 이상치 제거 평균 -> 비대칭 EMA)
 */

#include "bin_filter.h"

/* 4개 수거함 필터 인스턴스 (정적 할당으로 힙 메모리 파편화 원천 방지) */
static BinFilter s_bins[BIN_COUNT];

/* ---------------------------------------------------- */
/* 내부 헬퍼 함수                                       */
/* ---------------------------------------------------- */

/**
 * @brief  작은 크기 정렬에 최적화된 삽입 정렬(Insertion Sort) - 이상치 판단용 중앙값 계산에만 씀
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

/**
 * @brief  최근 샘플들 중, 중앙값에서 OUTLIER_THRESHOLD_CM 넘게 벗어난 값들을 빼고 평균 계산
 */
static float Average_Excluding_Outliers(const float *samples, uint8_t count)
{
    float sort_tmp[SAMPLE_WINDOW_SIZE];
    for (uint8_t i = 0; i < count; i++)
    {
        sort_tmp[i] = samples[i];
    }
    Sort_Samples(sort_tmp, count);
    float median = sort_tmp[count / 2];

    float sum = 0.0f;
    uint8_t used = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        float diff = samples[i] - median;
        if (diff < 0.0f) diff = -diff;

        if (diff <= OUTLIER_THRESHOLD_CM)
        {
            sum += samples[i];
            used++;
        }
    }

    if (used == 0)
    {
        /* 전부 이상치로 빠지는 극단적 경우(이론상 median 자신은 항상 포함되므로 실제로는 안 생김) */
        return median;
    }

    return sum / (float)used;
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
        // 적재율은 반드시 0%에서 시작 (통이 비어있다고 가정) - 이후 EMA로 서서히 실제값에 수렴
        s_bins[i].filtered_dist_cm = DEFAULT_BIN_EMPTY_CM;
        s_bins[i].filtered_percent = 0.0f;
        s_bins[i].is_initialized = false;

        for (int j = 0; j < SAMPLE_WINDOW_SIZE; j++)
        {
            s_bins[i].sample_buf[j] = DEFAULT_BIN_EMPTY_CM;
        }
    }
}

// 캘리브레이션 값(empty_cm/full_cm)은 그대로 두고, 나머지 필터 상태만 초기화 (0%/샘플버퍼/블랭킹 다 리셋)
void BinFilter_Reset(BinType bin)
{
    if ((int)bin < 0 || (int)bin >= BIN_COUNT) return;

    BinFilter *b = &s_bins[bin];

    float saved_empty = b->empty_cm;
    float saved_full  = b->full_cm;

    b->blanking_until_tick = 0U;
    b->sample_count = 0U;
    b->buf_idx = 0U;
    b->filtered_dist_cm = saved_empty;
    b->filtered_percent = 0.0f;
    b->is_initialized = false;

    for (int j = 0; j < SAMPLE_WINDOW_SIZE; j++)
    {
        b->sample_buf[j] = saved_empty;
    }

    b->empty_cm = saved_empty;
    b->full_cm  = saved_full;
}

void BinFilter_Reset_All(void)
{
    for (int i = 0; i < BIN_COUNT; i++)
    {
        BinFilter_Reset((BinType)i);
    }
}

void BinFilter_Config_Distance(BinType bin, float empty_cm, float full_cm)
{
    if ((int)bin < 0 || (int)bin >= BIN_COUNT) return;
    if (empty_cm <= full_cm) return; // 유효성 검사

    s_bins[bin].empty_cm = empty_cm;
    s_bins[bin].full_cm  = full_cm;
    // 캘리브레이션이 바뀌면 기준 거리도 같이 맞춰서, 부팅 직후 0%가 진짜 "빈 통 거리"를 의미하게 함
    s_bins[bin].filtered_dist_cm = empty_cm;
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

    /* 투입 진행 중 낙하 중 센서 가림 현상 무시 -> 2초동안 이전 값 리턴*/
    if (current_tick_ms < bin->blanking_until_tick)
    {
        return bin->filtered_percent; // 이전 상태 그대로 유지
    }

    /* 예외 처리-센서 타임아웃(0cm 이하) 또는 20cm 초과 시 이전 값 유지 */
    if (raw_dist_cm <= 0.0f || raw_dist_cm > (bin->empty_cm + 20.0f))
    {
        return bin->filtered_percent;
    }

    /* 10개 샘플 링버퍼에 저장 */
    bin->sample_buf[bin->buf_idx] = raw_dist_cm;
    bin->buf_idx = (bin->buf_idx + 1U) % SAMPLE_WINDOW_SIZE;
    if (bin->sample_count < SAMPLE_WINDOW_SIZE)
    {
        bin->sample_count++;
    }

    /* 중앙값 기준 ±OUTLIER_THRESHOLD_CM 벗어난 값 제외하고 평균 */
    float avg_dist = Average_Excluding_Outliers(bin->sample_buf, bin->sample_count);
    float current_raw_pct = Convert_Distance_To_Percent(bin, avg_dist);

    /* 비대칭 지수 이동 평균 (EMA) 필터
     * - filtered_percent는 BinFilter_Init()에서 0.0f로 시작하므로,
     *   첫 유효 측정치도 예외 없이 이 EMA를 거쳐서 "0%에서부터 서서히" 올라간다.
     *   (예전 코드는 첫 측정치를 즉시 반영해서 0%를 안 거치고 바로 뛰었음 -> 제거)
     */
    bin->is_initialized = true; // 참고용 플래그로만 남김 (동작에는 영향 없음)

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
        bin->filtered_dist_cm = (alpha * avg_dist)        + ((1.0f - alpha) * bin->filtered_dist_cm);
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