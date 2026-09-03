package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class KioskBindRequest(
    @Json(name = "bin_id")
    val binId: Int,
    @Json(name = "user_id")
    val userId: Int
)
