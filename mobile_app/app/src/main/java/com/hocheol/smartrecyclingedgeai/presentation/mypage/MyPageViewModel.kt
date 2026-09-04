package com.hocheol.smartrecyclingedgeai.presentation.mypage

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.repository.AuthRepository
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import com.hocheol.smartrecyclingedgeai.domain.model.EcoCalculator
import com.hocheol.smartrecyclingedgeai.domain.model.EcoLevel
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.firstOrNull
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class MyPageViewModel @Inject constructor(
    private val kioskRepository: KioskRepository,
    private val authRepository: AuthRepository,
    private val sessionManager: SessionManager
) : ViewModel() {

    private val _uiState = MutableStateFlow(MyPageUiState())
    val uiState: StateFlow<MyPageUiState> = _uiState.asStateFlow()

    init {
        observeUserSession()
    }

    private fun observeUserSession() {
        viewModelScope.launch {
            sessionManager.userIdFlow.collectLatest { userId ->
                if (userId != null) {
                    fetchMyPageData(userId)
                } else {
                    _uiState.update { MyPageUiState() }
                }
            }
        }
        viewModelScope.launch {
            sessionManager.userPointsFlow.collectLatest { points ->
                if (points != null) {
                    _uiState.update { state ->
                        state.copy(user = state.user?.copy(points = points))
                    }
                }
            }
        }
    }

    fun loadMyPageData() {
        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId != null) {
                fetchMyPageData(userId)
            }
        }
    }

    private suspend fun fetchMyPageData(userId: Int) {
        _uiState.update { it.copy(isLoading = true, errorMessage = null) }

        val userResult = kioskRepository.getUserInfo(userId)
        val user = userResult.getOrNull()

        val logsResult = kioskRepository.getUserLogs(userId)
        val logs = logsResult.getOrDefault(emptyList())

        val totalCount = logs.size
        val totalCarbonG = logs.sumOf { it.carbonSavedG }
        val ecoLevel = EcoLevel.fromCount(totalCount)
        val pineTrees = EcoCalculator.calculatePineTrees(totalCarbonG)

        _uiState.update {
            it.copy(
                isLoading = false,
                user = user,
                totalRecycleCount = totalCount,
                ecoLevel = ecoLevel,
                pineTreesSaved = pineTrees
            )
        }
    }

    fun showLogoutDialog() {
        _uiState.update { it.copy(isShowLogoutDialog = true) }
    }

    fun dismissLogoutDialog() {
        _uiState.update { it.copy(isShowLogoutDialog = false) }
    }

    fun logout(onLoggedOut: () -> Unit) {
        viewModelScope.launch {
            authRepository.clearSession()
            _uiState.update { MyPageUiState() }
            onLoggedOut()
        }
    }

    fun clearErrorMessage() {
        _uiState.update { it.copy(errorMessage = null) }
    }
}
