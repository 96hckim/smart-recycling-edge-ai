package com.hocheol.smartrecyclingedgeai.data.model.response

import com.squareup.moshi.Json
import com.squareup.moshi.JsonClass

@JsonClass(generateAdapter = true)
data class RecycleCompleteEvent(
    @Json(name = "event")
    val event: String,
    @Json(name = "user_id")
    val userId: Int,
    @Json(name = "paper_count")
    val paperCount: Int = 0,
    @Json(name = "can_count")
    val canCount: Int = 0,
    @Json(name = "pet_count")
    val petCount: Int = 0,
    @Json(name = "vinyl_count")
    val vinylCount: Int = 0,
    @Json(name = "earned_points")
    val earnedPoints: Int = 0,
    @Json(name = "carbon_saved_g")
    val carbonSavedG: Double = 0.0,
    @Json(name = "total_points")
    val totalPoints: Int = 0
)
