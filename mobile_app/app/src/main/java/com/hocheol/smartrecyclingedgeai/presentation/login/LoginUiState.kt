package com.hocheol.smartrecyclingedgeai.presentation.login

import androidx.compose.runtime.Immutable

/**
 * 로그인 화면 불변(Immutable) UI 상태
 */
@Immutable
data class LoginUiState(
    val phone: String = "",
    val name: String = "",
    val phoneError: String? = null,
    val isLoading: Boolean = false,
    val isCheckingAutoLogin: Boolean = true,
    val errorMessage: String? = null,
    val isLoggedIn: Boolean = false
)
