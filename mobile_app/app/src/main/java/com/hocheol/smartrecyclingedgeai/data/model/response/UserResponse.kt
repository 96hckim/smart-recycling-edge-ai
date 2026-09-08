package com.hocheol.smartrecyclingedgeai.data.model.response

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class UserResponse(
    @Json(name = "id")
    val id: Int,
    @Json(name = "phone")
    val phone: String,
    @Json(name = "name")
    val name: String? = "회원",
    @Json(name = "points")
    val points: Int,
    @Json(name = "created_at")
    val createdAt: String
)
