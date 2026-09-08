package com.hocheol.smartrecyclingedgeai.presentation.history

import androidx.compose.runtime.Immutable
import com.hocheol.smartrecyclingedgeai.domain.model.RecycleLog

@Immutable
data class HistoryUiState(
    val logs: List<RecycleLog> = emptyList(),
    val isLoading: Boolean = false,
    val isRefreshing: Boolean = false,
    val errorMessage: String? = null
)
