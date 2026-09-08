package com.hocheol.smartrecyclingedgeai.domain.model

import androidx.compose.runtime.Immutable

/**
 * 수거함 분리배출 완결 정산 결과 도메인 모델
 */
@Immutable
data class RecycleResult(
    val userId: Int,
    val paperCount: Int,
    val canCount: Int,
    val petCount: Int,
    val vinylCount: Int,
    val earnedPoints: Int,
    val carbonSavedG: Double,
    val totalPoints: Int
)
