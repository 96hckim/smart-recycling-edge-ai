package com.hocheol.smartrecyclingedgeai.domain.model

import androidx.compose.runtime.Immutable

@Immutable
data class User(
    val id: Int,
    val phone: String,
    val name: String,
    val points: Int,
    val createdAt: String
)
