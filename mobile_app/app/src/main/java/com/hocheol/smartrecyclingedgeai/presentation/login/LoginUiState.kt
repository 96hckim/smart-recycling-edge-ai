package com.hocheol.smartrecyclingedgeai.presentation.login

data class LoginUiState(
    val phone: String = "",
    val name: String = "",
    val phoneError: String? = null,
    val isLoading: Boolean = false,
    val isCheckingAutoLogin: Boolean = true,
    val errorMessage: String? = null,
    val isLoggedIn: Boolean = false
)
