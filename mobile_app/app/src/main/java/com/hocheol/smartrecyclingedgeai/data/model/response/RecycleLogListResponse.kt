package com.hocheol.smartrecyclingedgeai.data.model.response

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class RecycleLogListResponse(
    @Json(name = "user_id")
    val userId: Int,
    @Json(name = "total_count")
    val totalCount: Int = 0,
    @Json(name = "logs")
    val logs: List<RecycleLogItemResponse> = emptyList()
)
