package com.hocheol.smartrecyclingedgeai.data.repository

import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.model.request.KioskBindRequest
import com.hocheol.smartrecyclingedgeai.data.model.response.KioskBindResponse
import com.hocheol.smartrecyclingedgeai.data.remote.KioskApiService
import com.hocheol.smartrecyclingedgeai.data.remote.KioskWebSocketManager
import com.hocheol.smartrecyclingedgeai.domain.model.RecycleResult
import com.hocheol.smartrecyclingedgeai.domain.model.User
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

class KioskRepository(
    private val apiService: KioskApiService,
    private val webSocketManager: KioskWebSocketManager,
    private val sessionManager: SessionManager
) {
    val userIdFlow: Flow<Int?> = sessionManager.userIdFlow

    val recycleResultFlow: Flow<RecycleResult> = webSocketManager.recycleEventFlow.map { event ->
        RecycleResult(
            userId = event.userId,
            paperCount = event.paperCount,
            canCount = event.canCount,
            petCount = event.petCount,
            vinylCount = event.vinylCount,
            earnedPoints = event.earnedPoints,
            carbonSavedG = event.carbonSavedG,
            totalPoints = event.totalPoints
        )
    }

    suspend fun getUserInfo(userId: Int): Result<User> {
        return try {
            val response = apiService.getUserInfo(userId)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                val user = User(
                    id = body.id,
                    phone = body.phone,
                    name = body.name,
                    points = body.points,
                    createdAt = body.createdAt
                )
                sessionManager.saveSession(
                    userId = user.id,
                    userName = user.name,
                    phone = user.phone
                )
                Result.success(user)
            } else {
                Result.failure(Exception("유저 정보 조회 실패 (${response.code()})"))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    suspend fun bindKiosk(binId: Int, userId: Int): Result<KioskBindResponse> {
        return try {
            val request = KioskBindRequest(binId = binId, userId = userId)
            val response = apiService.bindKiosk(request)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                Result.success(body)
            } else {
                val errorBody = response.errorBody()?.string() ?: "키오스크 바인딩 실패"
                Result.failure(Exception("바인딩 실패 (${response.code()}): $errorBody"))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    fun connectKioskWebSocket(binId: Int) {
        webSocketManager.connect(binId)
    }

    fun disconnectKioskWebSocket() {
        webSocketManager.disconnect()
    }
}
