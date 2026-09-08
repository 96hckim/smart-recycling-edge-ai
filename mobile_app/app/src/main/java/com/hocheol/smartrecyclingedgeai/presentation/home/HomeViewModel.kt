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

/**
 * 대시보드 메인 화면 뷰모델
 * 수거함 QR 스캔, 외부 딥링크 처리, 실시간 웹소켓 배출 완결 이벤트 수신을 관장합니다.
 */
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
        observeCancelEvents()
    }

    /**
     * 유저 세션 및 잔여 포인트를 반응형 관찰하여 홈 화면 UI 자동 업데이트
     */

    private fun observeUserSession() {
        viewModelScope.launch {
            sessionManager.userIdFlow.collectLatest { userId ->
                if (userId != null) {
                    fetchUserInfo(userId)
                    // 로그아웃 상태에서 들어왔던 대기 딥링크가 존재하는 경우 로그인 완료 시 즉시 키오스크 연결
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

    /**
     * 웹소켓을 통한 실시간 분리배출 정산 완결 이벤트(RECYCLE_COMPLETE) 수신
     */
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

    /**
     * 키오스크 측에서 투입 취소 시(SESSION_CANCELLED) 모바일 세션 자동 닫기
     */
    private fun observeCancelEvents() {
        viewModelScope.launch {
            kioskRepository.sessionCancelFlow.collect {
                kioskRepository.disconnectKioskWebSocket()
                _uiState.update {
                    it.copy(
                        isKioskActive = false,
                        activeBinId = null
                    )
                }
            }
        }
    }

    fun openQRScanner() {

        _uiState.update { it.copy(isScanningQR = true) }
    }

    fun closeQRScanner() {
        _uiState.update { it.copy(isScanningQR = false) }
    }

    /**
     * 앱 내 카메라로 QR 스캔 성공 시 키오스크 바인딩 실행
     */
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

    /**
     * 기본 카메라 앱 딥링크(smartrecycle://kiosk/auth?bin_id=1) 진입 처리
     */
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
                        // 이미 동일 수거함 세션이 연결 중인 경우 중복 연사 방지
                        if (_uiState.value.isKioskActive && _uiState.value.activeBinId == binId) return@launch
                        if (_uiState.value.isKioskBinding) return@launch

                        _uiState.update { it.copy(isKioskBinding = true) }
                        bindKiosk(binId = binId, userId = userId)
                    } else {
                        // 로그아웃 상태 진입 시 가짜 바인딩 차단 및 대기 세션 세팅
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
            } catch (_: Exception) {
                null
            }
        }
        return rawContent.toIntOrNull()
    }

    /**
     * 키오스크 수거함 바인딩 API 호출 및 실시간 웹소켓 파이프라인 개설
     */
    private fun bindKiosk(binId: Int, userId: Int? = null) {
        viewModelScope.launch {
            val actualUserId = userId ?: sessionManager.userIdFlow.firstOrNull()
            if (actualUserId == null) {
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
        val binId = _uiState.value.activeBinId
        val userId = _uiState.value.user?.id
        if (binId != null) {
            viewModelScope.launch {
                kioskRepository.cancelKiosk(binId, userId)
            }
        }
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

    override fun onCleared() {
        super.onCleared()
        kioskRepository.disconnectKioskWebSocket()
    }
}
