package com.hocheol.smartrecyclingedgeai.data.model.response

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

/**
 * 키오스크 수거함 바인딩 응답 DTO
 */
@JsonClass(generateAdapter = true)
data class KioskBindResponse(
    @Json(name = "status")
    val status: String = "SUCCESS",
    @Json(name = "message")
    val message: String,
    @Json(name = "bin_id")
    val binId: Int,
    @Json(name = "user_id")
    val userId: Int
)
