package com.hocheol.smartrecyclingedgeai.domain.model

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
