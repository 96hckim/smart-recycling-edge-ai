package com.hocheol.smartrecyclingedgeai.presentation.mypage

import androidx.compose.runtime.Immutable
import com.hocheol.smartrecyclingedgeai.domain.model.EcoLevel
import com.hocheol.smartrecyclingedgeai.domain.model.User

@Immutable
data class MyPageUiState(
    val user: User? = null,
    val totalRecycleCount: Int = 0,
    val ecoLevel: EcoLevel = EcoLevel.SPROUT,
    val pineTreesSaved: Double = 0.0,
    val isLoading: Boolean = false,
    val isShowLogoutDialog: Boolean = false,
    val errorMessage: String? = null
)
