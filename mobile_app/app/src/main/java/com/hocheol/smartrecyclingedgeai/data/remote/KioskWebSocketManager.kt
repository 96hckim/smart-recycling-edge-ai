package com.hocheol.smartrecyclingedgeai.data.remote

import com.hocheol.smartrecyclingedgeai.data.model.response.RecycleCompleteEvent
import com.hocheol.smartrecyclingedgeai.utils.Constants
import com.squareup.moshi.Moshi
import com.squareup.moshi.kotlin.reflect.KotlinJsonAdapterFactory
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import javax.inject.Inject
import javax.inject.Singleton
import kotlin.time.Duration.Companion.milliseconds

/**
 * WebSocket 연결 상태 모니터링을 위한 Sealed Interface
 */
sealed interface WebSocketConnectionState {
    object Disconnected : WebSocketConnectionState
    object Connecting : WebSocketConnectionState
    object Connected : WebSocketConnectionState
    data class Error(val message: String) : WebSocketConnectionState
}

/**
 * 키오스크 실시간 투입 완결 이벤트를 수신하는 OkHttp WebSocket 매니저
 * 네트워크 흔들림 시 Exponential Backoff 기반 자동 재연결 알고리즘을 수행합니다.
 */
@Singleton
class KioskWebSocketManager @Inject constructor(
    private val okHttpClient: OkHttpClient
) {
    private var webSocket: WebSocket? = null
    private var currentBinId: Int? = null
    private var reconnectJob: Job? = null
    private var isUserDisconnect = false

    private val scope = CoroutineScope(Dispatchers.IO)

    private val _connectionState =
        MutableStateFlow<WebSocketConnectionState>(WebSocketConnectionState.Disconnected)
    val connectionState: StateFlow<WebSocketConnectionState> = _connectionState.asStateFlow()

    // 실시간 분리배출 완결 이벤트 수신 파이프라인
    private val _recycleEventFlow = MutableSharedFlow<RecycleCompleteEvent>(extraBufferCapacity = 1)
    val recycleEventFlow: SharedFlow<RecycleCompleteEvent> = _recycleEventFlow.asSharedFlow()

    // 실시간 세션 중도 취소 이벤트 수신 파이프라인
    private val _sessionCancelFlow = MutableSharedFlow<Unit>(extraBufferCapacity = 1)
    val sessionCancelFlow: SharedFlow<Unit> = _sessionCancelFlow.asSharedFlow()

    private val moshi = Moshi.Builder().addLast(KotlinJsonAdapterFactory()).build()
    private val eventAdapter = moshi.adapter(RecycleCompleteEvent::class.java)


    /**
     * 특정 키오스크 수거함(binId)에 실시간 웹소켓 세션 연결
     */
    fun connect(binId: Int) {
        isUserDisconnect = false
        currentBinId = binId
        reconnectJob?.cancel()

        _connectionState.value = WebSocketConnectionState.Connecting

        val url = "${Constants.WS_BASE_URL}ws/kiosk/$binId/mobile"
        val request = Request.Builder().url(url).build()

        webSocket?.close(1000, "Reconnecting")
        webSocket = okHttpClient.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                _connectionState.value = WebSocketConnectionState.Connected
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                try {
                    val event = eventAdapter.fromJson(text)
                    if (event != null) {
                        when (event.event) {
                            Constants.EVENT_RECYCLE_COMPLETE -> _recycleEventFlow.tryEmit(event)
                            Constants.EVENT_SESSION_CANCELLED -> _sessionCancelFlow.tryEmit(Unit)
                        }
                    }
                } catch (_: Exception) {
                }
            }


            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                _connectionState.value =
                    WebSocketConnectionState.Error(t.localizedMessage ?: "WebSocket Failure")
                scheduleReconnect()
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                if (!isUserDisconnect) {
                    _connectionState.value = WebSocketConnectionState.Disconnected
                    scheduleReconnect()
                }
            }
        })
    }

    /**
     * 연결 예외 단절 시 Exponential Backoff 기반 지수 재연결 지연시도 (1s, 2s, 4s...)
     */
    private fun scheduleReconnect() {
        if (isUserDisconnect) return
        val binId = currentBinId ?: return

        reconnectJob?.cancel()
        reconnectJob = scope.launch {
            var delayMs = 1000L
            repeat(3) {
                if (isUserDisconnect) return@launch
                delay(delayMs.milliseconds)
                _connectionState.value = WebSocketConnectionState.Connecting
                connect(binId)
                delayMs *= 2
            }
        }
    }

    /**
     * 세션 완결 또는 사용자에 의한 명시적 웹소켓 연결 해제
     */
    fun disconnect() {
        isUserDisconnect = true
        reconnectJob?.cancel()
        currentBinId = null
        webSocket?.close(1000, "User finished or closed session")
        webSocket = null
        _connectionState.value = WebSocketConnectionState.Disconnected
    }
}
