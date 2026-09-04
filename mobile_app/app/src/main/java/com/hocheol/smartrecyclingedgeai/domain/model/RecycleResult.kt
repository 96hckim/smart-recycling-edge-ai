package com.hocheol.smartrecyclingedgeai.domain.model

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
