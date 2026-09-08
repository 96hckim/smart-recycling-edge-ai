package com.hocheol.smartrecyclingedgeai.domain.model

import androidx.compose.runtime.Immutable

/**
 * 사용자 프로필 및 보유 포인트 도메인 모델
 */
@Immutable
data class User(
    val id: Int,
    val phone: String,
    val name: String,
    val points: Int,
    val createdAt: String
)
