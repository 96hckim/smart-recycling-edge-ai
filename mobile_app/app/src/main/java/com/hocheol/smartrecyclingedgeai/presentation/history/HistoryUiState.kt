package com.hocheol.smartrecyclingedgeai.presentation.history

import com.hocheol.smartrecyclingedgeai.domain.model.RecycleLog

data class HistoryUiState(
    val logs: List<RecycleLog> = emptyList(),
    val isLoading: Boolean = false,
    val isRefreshing: Boolean = false,
    val errorMessage: String? = null
)
