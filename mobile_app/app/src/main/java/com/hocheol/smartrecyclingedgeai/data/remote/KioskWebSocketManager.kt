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

sealed interface WebSocketConnectionState {
    object Disconnected : WebSocketConnectionState
    object Connecting : WebSocketConnectionState
    object Connected : WebSocketConnectionState
    data class Error(val message: String) : WebSocketConnectionState
}

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

    private val _recycleEventFlow = MutableSharedFlow<RecycleCompleteEvent>(extraBufferCapacity = 1)
    val recycleEventFlow: SharedFlow<RecycleCompleteEvent> = _recycleEventFlow.asSharedFlow()

    private val moshi = Moshi.Builder().addLast(KotlinJsonAdapterFactory()).build()
    private val eventAdapter = moshi.adapter(RecycleCompleteEvent::class.java)

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
                    if (event != null && event.event == Constants.EVENT_RECYCLE_COMPLETE) {
                        _recycleEventFlow.tryEmit(event)
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
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

    private fun scheduleReconnect() {
        if (isUserDisconnect) return
        val binId = currentBinId ?: return

        reconnectJob?.cancel()
        reconnectJob = scope.launch {
            var delayMs = 1000L
            repeat(3) { attempt ->
                if (isUserDisconnect) return@launch
                delay(delayMs.milliseconds)
                _connectionState.value = WebSocketConnectionState.Connecting
                connect(binId)
                delayMs *= 2
            }
        }
    }

    fun disconnect() {
        isUserDisconnect = true
        reconnectJob?.cancel()
        currentBinId = null
        webSocket?.close(1000, "User finished or closed session")
        webSocket = null
        _connectionState.value = WebSocketConnectionState.Disconnected
    }
}
