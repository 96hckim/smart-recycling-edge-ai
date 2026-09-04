package com.hocheol.smartrecyclingedgeai.data.model.response

import com.squareup.moshi.Json

data class PointDeductResponse(
    @Json(name = "status") val status: String,
    @Json(name = "user_id") val userId: Int,
    @Json(name = "deducted_amount") val deductedAmount: Int,
    @Json(name = "remaining_points") val remainingPoints: Int,
    @Json(name = "description") val description: String
)
