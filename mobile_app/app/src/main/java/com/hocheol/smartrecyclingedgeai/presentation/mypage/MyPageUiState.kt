package com.hocheol.smartrecyclingedgeai.presentation.mypage

import com.hocheol.smartrecyclingedgeai.domain.model.EcoLevel
import com.hocheol.smartrecyclingedgeai.domain.model.User

data class MyPageUiState(
    val user: User? = null,
    val totalRecycleCount: Int = 0,
    val ecoLevel: EcoLevel = EcoLevel.SPROUT,
    val pineTreesSaved: Double = 0.0,
    val isLoading: Boolean = false,
    val isShowLogoutDialog: Boolean = false,
    val errorMessage: String? = null
)
