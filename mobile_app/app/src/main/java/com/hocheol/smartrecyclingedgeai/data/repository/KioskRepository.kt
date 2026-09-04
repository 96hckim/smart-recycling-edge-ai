package com.hocheol.smartrecyclingedgeai.data.repository

import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.model.request.KioskBindRequest
import com.hocheol.smartrecyclingedgeai.data.model.request.PointDeductRequest
import com.hocheol.smartrecyclingedgeai.data.model.response.KioskBindResponse
import com.hocheol.smartrecyclingedgeai.data.model.response.PointDeductResponse
import com.hocheol.smartrecyclingedgeai.data.remote.KioskApiService
import com.hocheol.smartrecyclingedgeai.data.remote.KioskWebSocketManager
import com.hocheol.smartrecyclingedgeai.domain.model.RecycleLog
import com.hocheol.smartrecyclingedgeai.domain.model.RecycleResult
import com.hocheol.smartrecyclingedgeai.domain.model.User
import com.squareup.moshi.Moshi
import com.squareup.moshi.kotlin.reflect.KotlinJsonAdapterFactory
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class KioskRepository @Inject constructor(
    private val apiService: KioskApiService,
    private val webSocketManager: KioskWebSocketManager,
    private val sessionManager: SessionManager
) {
    val userIdFlow: Flow<Int?> = sessionManager.userIdFlow
    val userPointsFlow: Flow<Int?> = sessionManager.userPointsFlow

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
                    phone = user.phone,
                    points = user.points
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

    suspend fun getUserLogs(userId: Int): Result<List<RecycleLog>> {
        return try {
            val response = apiService.getUserLogs(userId)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                val logs = body.logs.map { item ->
                    RecycleLog(
                        id = item.id,
                        binId = item.binId,
                        canCount = item.canCount,
                        petCount = item.petCount,
                        paperCount = item.paperCount,
                        vinylCount = item.vinylCount,
                        carbonSavedG = item.carbonSavedG,
                        earnedPoints = item.earnedPoints,
                        createdAt = item.createdAt
                    )
                }
                Result.success(logs)
            } else {
                Result.failure(Exception("배출 내역 조회 실패 (${response.code()})"))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    suspend fun deductPoints(
        userId: Int,
        amount: Int,
        description: String
    ): Result<PointDeductResponse> {
        return try {
            val request = PointDeductRequest(
                userId = userId,
                amount = amount,
                description = description
            )
            val response = apiService.deductPoints(request)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                sessionManager.updatePoints(body.remainingPoints)
                Result.success(body)
            } else {
                val errorString = response.errorBody()?.string() ?: ""
                val errorMessage = parseErrorMessage(errorString, response.code())
                Result.failure(Exception(errorMessage))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류가 발생했습니다: ${e.localizedMessage}"))
        }
    }

    private fun parseErrorMessage(errorString: String, statusCode: Int): String {
        return try {
            val moshi = Moshi.Builder()
                .addLast(KotlinJsonAdapterFactory())
                .build()
            val jsonAdapter = moshi.adapter(Map::class.java)
            val map = jsonAdapter.fromJson(errorString)
            val detail = map?.get("detail") as? String
            detail ?: "포인트 차감 실패 ($statusCode)"
        } catch (e: Exception) {
            if (errorString.isNotBlank()) errorString else "포인트 차감 실패 ($statusCode)"
        }
    }

    fun connectKioskWebSocket(binId: Int) {
        webSocketManager.connect(binId)
    }

    fun disconnectKioskWebSocket() {
        webSocketManager.disconnect()
    }
}
