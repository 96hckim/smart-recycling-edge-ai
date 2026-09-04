package com.hocheol.smartrecyclingedgeai.data.remote

import com.hocheol.smartrecyclingedgeai.data.model.response.RecycleCompleteEvent
import com.hocheol.smartrecyclingedgeai.utils.Constants
import com.squareup.moshi.Moshi
import com.squareup.moshi.kotlin.reflect.KotlinJsonAdapterFactory
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class KioskWebSocketManager @Inject constructor(
    private val okHttpClient: OkHttpClient
) {
    private var webSocket: WebSocket? = null

    private val _recycleEventFlow = MutableSharedFlow<RecycleCompleteEvent>(extraBufferCapacity = 1)
    val recycleEventFlow: SharedFlow<RecycleCompleteEvent> = _recycleEventFlow.asSharedFlow()

    private val moshi = Moshi.Builder().addLast(KotlinJsonAdapterFactory()).build()
    private val eventAdapter = moshi.adapter(RecycleCompleteEvent::class.java)

    fun connect(binId: Int) {
        disconnect()

        val url = "${Constants.WS_BASE_URL}ws/kiosk/$binId/mobile"
        val request = Request.Builder().url(url).build()

        webSocket = okHttpClient.newWebSocket(request, object : WebSocketListener() {
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
                t.printStackTrace()
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                // Connection closed
            }
        })
    }

    fun disconnect() {
        webSocket?.close(1000, "User finished or closed session")
        webSocket = null
    }
}
