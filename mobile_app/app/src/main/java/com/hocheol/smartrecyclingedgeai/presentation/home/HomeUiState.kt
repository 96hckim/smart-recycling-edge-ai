package com.hocheol.smartrecyclingedgeai.presentation.home

import com.hocheol.smartrecyclingedgeai.domain.model.RecycleResult
import com.hocheol.smartrecyclingedgeai.domain.model.User

data class HomeUiState(
    val user: User? = null,
    val isLoading: Boolean = false,
    val isRefreshing: Boolean = false,
    val isScanningQR: Boolean = false,
    val isKioskBinding: Boolean = false,
    val isKioskActive: Boolean = false,
    val activeBinId: Int? = null,
    val pendingDeeplinkBinId: Int? = null,
    val recycleResult: RecycleResult? = null,
    val errorMessage: String? = null,
    val infoMessage: String? = null
)
