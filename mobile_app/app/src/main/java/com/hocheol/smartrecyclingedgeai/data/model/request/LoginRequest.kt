package com.hocheol.smartrecyclingedgeai.data.model.request

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class LoginRequest(
    @Json(name = "phone")
    val phone: String,
    @Json(name = "name")
    val name: String? = null
)
