package com.hocheol.smartrecyclingedgeai.presentation.home

import android.net.Uri
import androidx.core.net.toUri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import com.hocheol.smartrecyclingedgeai.utils.Constants
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
class HomeViewModel @Inject constructor(
    private val kioskRepository: KioskRepository,
    private val sessionManager: SessionManager
) : ViewModel() {

    private val _uiState = MutableStateFlow(HomeUiState())
    val uiState: StateFlow<HomeUiState> = _uiState.asStateFlow()

    init {
        observeUserSession()
        observeRecycleEvents()
    }

    private fun observeUserSession() {
        viewModelScope.launch {
            sessionManager.userIdFlow.collectLatest { userId ->
                if (userId != null) {
                    fetchUserInfo(userId)
                    // 로그아웃 상태에서 들어왔던 대기 중인 딥링크가 있는 경우, 로그인 완료 시 즉시 키오스크 바인딩 수행!
                    val pendingBinId = _uiState.value.pendingDeeplinkBinId
                    if (pendingBinId != null) {
                        _uiState.update {
                            it.copy(
                                pendingDeeplinkBinId = null,
                                isKioskBinding = true
                            )
                        }
                        bindKiosk(binId = pendingBinId, userId = userId)
                    }
                } else {
                    _uiState.update { HomeUiState() }
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

    private fun fetchUserInfo(userId: Int) {
        viewModelScope.launch {
            _uiState.update { it.copy(isLoading = true, errorMessage = null) }
            val result = kioskRepository.getUserInfo(userId)
            result.onSuccess { user ->
                _uiState.update { it.copy(isLoading = false, isRefreshing = false, user = user) }
            }.onFailure { error ->
                _uiState.update {
                    it.copy(isLoading = false, isRefreshing = false, errorMessage = error.message)
                }
            }
        }
    }

    fun refresh() {
        _uiState.update { it.copy(isRefreshing = true) }
        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId != null) {
                val result = kioskRepository.getUserInfo(userId)
                result.onSuccess { user ->
                    _uiState.update { it.copy(isRefreshing = false, user = user) }
                }.onFailure { error ->
                    _uiState.update { it.copy(isRefreshing = false, errorMessage = error.message) }
                }
            } else {
                _uiState.update { it.copy(isRefreshing = false) }
            }
        }
    }

    private fun observeRecycleEvents() {
        viewModelScope.launch {
            kioskRepository.recycleResultFlow.collect { result ->
                kioskRepository.disconnectKioskWebSocket()
                _uiState.update {
                    it.copy(
                        isKioskActive = false,
                        activeBinId = null,
                        recycleResult = result
                    )
                }
                refresh()
            }
        }
    }

    fun openQRScanner() {
        _uiState.update { it.copy(isScanningQR = true) }
    }

    fun closeQRScanner() {
        _uiState.update { it.copy(isScanningQR = false) }
    }

    fun handleScannedQrContent(rawContent: String) {
        val binId = parseBinId(rawContent)
        if (binId == null) {
            _uiState.update {
                it.copy(
                    isScanningQR = false,
                    errorMessage = "유효하지 않은 키오스크 QR 코드입니다."
                )
            }
            return
        }

        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId == null) {
                _uiState.update {
                    it.copy(
                        isScanningQR = false,
                        errorMessage = "키오스크 연결을 위해 로그인이 필요합니다."
                    )
                }
                return@launch
            }

            _uiState.update {
                it.copy(
                    isScanningQR = false,
                    isKioskBinding = true
                )
            }

            bindKiosk(binId = binId, userId = userId)
        }
    }

    fun handleDeeplink(uri: Uri) {
        val scheme = uri.scheme
        val host = uri.host
        val binIdParam = uri.getQueryParameter(Constants.PARAM_BIN_ID)

        if (scheme == Constants.DEEPLINK_SCHEME && host == Constants.DEEPLINK_HOST && binIdParam != null) {
            val binId = binIdParam.toIntOrNull()
            if (binId != null) {
                viewModelScope.launch {
                    val userId = sessionManager.userIdFlow.firstOrNull()
                    if (userId != null) {
                        // 로그인 상태: 즉시 키오스크 바인딩
                        if (_uiState.value.isKioskActive && _uiState.value.activeBinId == binId) return@launch
                        if (_uiState.value.isKioskBinding) return@launch

                        _uiState.update { it.copy(isKioskBinding = true) }
                        bindKiosk(binId = binId, userId = userId)
                    } else {
                        // 로그아웃 상태: 가짜 바인딩 API 절대 호출 금지! 대기 binId 세팅 및 안내 메시지 표출
                        _uiState.update {
                            it.copy(
                                pendingDeeplinkBinId = binId,
                                errorMessage = "키오스크 연결을 위해 로그인이 필요합니다."
                            )
                        }
                    }
                }
            }
        }
    }

    private fun parseBinId(rawContent: String): Int? {
        if (rawContent.startsWith("http") || rawContent.contains("://")) {
            return try {
                val uri = rawContent.toUri()
                uri.getQueryParameter(Constants.PARAM_BIN_ID)?.toIntOrNull()
            } catch (e: Exception) {
                null
            }
        }
        return rawContent.toIntOrNull()
    }

    private fun bindKiosk(binId: Int, userId: Int? = null) {
        viewModelScope.launch {
            val actualUserId = userId ?: sessionManager.userIdFlow.firstOrNull()
            if (actualUserId == null) {
                // 로그인 안 된 상태에서는 가짜 바인딩 금지!
                _uiState.update {
                    it.copy(
                        isKioskBinding = false,
                        pendingDeeplinkBinId = binId,
                        errorMessage = "키오스크 연결을 위해 로그인이 필요합니다."
                    )
                }
                return@launch
            }

            val result = kioskRepository.bindKiosk(binId = binId, userId = actualUserId)
            result.onSuccess {
                kioskRepository.connectKioskWebSocket(binId)
                _uiState.update { state ->
                    state.copy(
                        isKioskBinding = false,
                        isKioskActive = true,
                        activeBinId = binId,
                        pendingDeeplinkBinId = null
                    )
                }
            }.onFailure { error ->
                _uiState.update { state ->
                    state.copy(
                        isKioskBinding = false,
                        errorMessage = error.message
                    )
                }
            }
        }
    }

    fun dismissRecycleResultDialog() {
        kioskRepository.disconnectKioskWebSocket()
        _uiState.update {
            it.copy(
                recycleResult = null,
                isKioskActive = false,
                activeBinId = null
            )
        }
        refresh()
    }

    fun cancelKioskSession() {
        kioskRepository.disconnectKioskWebSocket()
        _uiState.update {
            it.copy(
                isKioskActive = false,
                activeBinId = null
            )
        }
    }

    fun clearErrorMessage() {
        _uiState.update { it.copy(errorMessage = null) }
    }
}
