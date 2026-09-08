package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class PointDeductRequest(
    @Json(name = "user_id") val userId: Int,
    @Json(name = "amount") val amount: Int,
    @Json(name = "description") val description: String
)
