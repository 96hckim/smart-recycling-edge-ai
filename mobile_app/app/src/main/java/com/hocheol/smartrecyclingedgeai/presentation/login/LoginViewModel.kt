package com.hocheol.smartrecyclingedgeai.presentation.login

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.repository.AuthRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class LoginViewModel @Inject constructor(
    private val repository: AuthRepository
) : ViewModel() {

    private val _uiState = MutableStateFlow(LoginUiState())
    val uiState: StateFlow<LoginUiState> = _uiState.asStateFlow()

    init {
        checkAutoLogin()
    }

    private fun checkAutoLogin() {
        viewModelScope.launch {
            val isLoggedIn = repository.isLoggedIn()
            _uiState.update {
                it.copy(
                    isCheckingAutoLogin = false,
                    isLoggedIn = isLoggedIn
                )
            }
        }
    }

    fun onPhoneChanged(newPhone: String) {
        val filtered = newPhone.filter { it.isDigit() }.take(11)
        _uiState.update {
            it.copy(
                phone = filtered,
                phoneError = null,
                errorMessage = null
            )
        }
    }

    fun onNameChanged(newName: String) {
        _uiState.update {
            it.copy(
                name = newName,
                errorMessage = null
            )
        }
    }

    fun login() {
        val currentState = _uiState.value
        if (currentState.isLoading) return

        if (currentState.phone.length < 10) {
            _uiState.update {
                it.copy(phoneError = "올바른 휴대폰 번호(10~11자리)를 입력해 주세요.")
            }
            return
        }

        _uiState.update {
            it.copy(
                isLoading = true,
                errorMessage = null,
                phoneError = null
            )
        }

        viewModelScope.launch {
            val result = repository.login(
                phone = currentState.phone,
                name = currentState.name
            )
            result.onSuccess {
                _uiState.update { state ->
                    state.copy(
                        isLoading = false,
                        isLoggedIn = true
                    )
                }
            }.onFailure { exception ->
                _uiState.update { state ->
                    state.copy(
                        isLoading = false,
                        errorMessage = exception.message ?: "알 수 없는 오류가 발생했습니다."
                    )
                }
            }
        }
    }

    fun clearErrorMessage() {
        _uiState.update { it.copy(errorMessage = null) }
    }

    fun logout() {
        viewModelScope.launch {
            repository.clearSession()
            _uiState.update {
                LoginUiState(isCheckingAutoLogin = false)
            }
        }
    }
}
