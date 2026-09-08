package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

/**
 * 키오스크 수거함 세션 중도 취소 요청 DTO
 */
@JsonClass(generateAdapter = true)
data class KioskCancelRequest(
    @Json(name = "bin_id")
    val binId: Int,
    @Json(name = "user_id")
    val userId: Int? = null,
    @Json(name = "reason")
    val reason: String = "USER_CANCELLED"
)

