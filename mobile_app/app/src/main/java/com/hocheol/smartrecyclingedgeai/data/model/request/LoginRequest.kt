package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

/**
 * 휴대폰 번호 기반 간편 로그인 요청 DTO
 */
@JsonClass(generateAdapter = true)
data class LoginRequest(
    @Json(name = "phone")
    val phone: String,
    @Json(name = "name")
    val name: String? = null
)
