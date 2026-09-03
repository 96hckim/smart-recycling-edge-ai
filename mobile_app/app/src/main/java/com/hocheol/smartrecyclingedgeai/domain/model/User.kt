package com.hocheol.smartrecyclingedgeai.domain.model

data class User(
    val id: Int,
    val phone: String,
    val name: String,
    val points: Int,
    val createdAt: String
)
