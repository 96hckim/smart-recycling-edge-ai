package com.hocheol.smartrecyclingedgeai.presentation.common

/**
 * UI 레이어에서 전달받아 처리하는 일회성(One-time) Side-Effect 이벤트 정의
 */
sealed interface UiEvent {
    data class ShowSnackbar(val message: String) : UiEvent
}
