package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json

data class PointDeductRequest(
    @Json(name = "user_id") val userId: Int,
    @Json(name = "amount") val amount: Int,
    @Json(name = "description") val description: String
)
