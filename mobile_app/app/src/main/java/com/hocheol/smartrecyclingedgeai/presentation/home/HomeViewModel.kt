package com.hocheol.smartrecyclingedgeai.presentation.home

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.remote.RetrofitClient
import com.hocheol.smartrecyclingedgeai.data.remote.KioskWebSocketManager
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import com.hocheol.smartrecyclingedgeai.utils.Constants
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.firstOrNull
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

class HomeViewModel(
    application: Application,
    private val kioskRepository: KioskRepository,
    private val sessionManager: SessionManager
) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(HomeUiState())
    val uiState: StateFlow<HomeUiState> = _uiState.asStateFlow()

    init {
        loadUserInfo()
        observeRecycleEvents()
    }

    fun loadUserInfo() {
        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull() ?: return@launch
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
                _uiState.update {
                    it.copy(
                        isKioskActive = false,
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

        _uiState.update {
            it.copy(
                isScanningQR = false,
                isKioskBinding = true
            )
        }

        bindKiosk(binId)
    }

    fun handleDeeplink(uri: Uri) {
        val scheme = uri.scheme
        val host = uri.host
        val binIdParam = uri.getQueryParameter(Constants.PARAM_BIN_ID)

        if (scheme == Constants.DEEPLINK_SCHEME && host == Constants.DEEPLINK_HOST && binIdParam != null) {
            val binId = binIdParam.toIntOrNull()
            if (binId != null) {
                _uiState.update { it.copy(isKioskBinding = true) }
                bindKiosk(binId)
            }
        }
    }

    private fun parseBinId(rawContent: String): Int? {
        if (rawContent.startsWith("http") || rawContent.contains("://")) {
            return try {
                val uri = Uri.parse(rawContent)
                uri.getQueryParameter(Constants.PARAM_BIN_ID)?.toIntOrNull()
            } catch (e: Exception) {
                null
            }
        }
        return rawContent.toIntOrNull()
    }

    private fun bindKiosk(binId: Int) {
        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull() ?: 1
            val result = kioskRepository.bindKiosk(binId = binId, userId = userId)
            result.onSuccess {
                kioskRepository.connectKioskWebSocket(binId)
                _uiState.update { state ->
                    state.copy(
                        isKioskBinding = false,
                        isKioskActive = true,
                        activeBinId = binId
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

    class Factory(private val application: Application) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            if (modelClass.isAssignableFrom(HomeViewModel::class.java)) {
                val sessionManager = SessionManager.getInstance(application)
                val wsManager = KioskWebSocketManager(RetrofitClient.okHttpClient)
                val repository = KioskRepository(
                    apiService = RetrofitClient.kioskApiService,
                    webSocketManager = wsManager,
                    sessionManager = sessionManager
                )
                return HomeViewModel(application, repository, sessionManager) as T
            }
            throw IllegalArgumentException("Unknown ViewModel class")
        }
    }
}
