package com.hocheol.smartrecyclingedgeai.domain.model

import androidx.compose.runtime.Immutable

/**
 * 과거 분리배출 상세 내역 도메인 모델
 */
@Immutable
data class RecycleLog(
    val id: Int,
    val binId: Int,
    val canCount: Int,
    val petCount: Int,
    val paperCount: Int,
    val vinylCount: Int,
    val carbonSavedG: Double,
    val earnedPoints: Int,
    val createdAt: String
)
