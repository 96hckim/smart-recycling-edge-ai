package com.hocheol.smartrecyclingedgeai.presentation.common

sealed interface UiEvent {
    data class ShowSnackbar(val message: String) : UiEvent
}
