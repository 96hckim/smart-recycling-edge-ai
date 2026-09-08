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
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

/**
 * 키오스크 수거함 바인딩, 실시간 배출 정산 이벤트, 포인트 차감 및 이력 관리 리포지토리
 */
@Singleton
class KioskRepository @Inject constructor(
    private val apiService: KioskApiService,
    private val webSocketManager: KioskWebSocketManager,
    private val sessionManager: SessionManager
) {
    val userIdFlow: Flow<Int?> = sessionManager.userIdFlow
    val userPointsFlow: Flow<Int?> = sessionManager.userPointsFlow

    // 실시간 웹소켓 푸시 수신 파이프라인
    val recycleResultFlow: Flow<RecycleResult> = webSocketManager.recycleEventFlow.map { event ->
        RecycleResult(
            userId = event.userId ?: 0,
            paperCount = event.paperCount,
            canCount = event.canCount,
            petCount = event.petCount,
            vinylCount = event.vinylCount,
            earnedPoints = event.earnedPoints,
            carbonSavedG = event.carbonSavedG,
            totalPoints = event.totalPoints ?: 0
        )
    }

    /**
     * 유저 프로필 및 최신 보유 포인트 조회 (성공 시 DataStore 세션 자동 동기화)
     */
    suspend fun getUserInfo(userId: Int): Result<User> {
        return try {
            val response = apiService.getUserInfo(userId)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                val userName = body.name ?: "회원"
                val user = User(
                    id = body.id,
                    phone = body.phone,
                    name = userName,
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
                val errorString = response.errorBody()?.string() ?: ""
                val errorMessage = parseErrorMessage(errorString, response.code(), "유저 정보 조회 실패")
                Result.failure(Exception(errorMessage))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    /**
     * 키오스크 수거함 QR 바인딩 요청
     */
    suspend fun bindKiosk(binId: Int, userId: Int): Result<KioskBindResponse> {
        return try {
            val request = KioskBindRequest(binId = binId, userId = userId)
            val response = apiService.bindKiosk(request)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                Result.success(body)
            } else {
                val errorString = response.errorBody()?.string() ?: ""
                val errorMessage = parseErrorMessage(errorString, response.code(), "키오스크 바인딩 실패")
                Result.failure(Exception(errorMessage))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    /**
     * 과거 분리배출 상세 이력 목록 조회
     */
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
                val errorString = response.errorBody()?.string() ?: ""
                val errorMessage = parseErrorMessage(errorString, response.code(), "배출 내역 조회 실패")
                Result.failure(Exception(errorMessage))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류: ${e.localizedMessage}"))
        }
    }

    /**
     * 상점 상품 교환을 위한 포인트 차감 (성공 시 DataStore 세션 잔여 포인트 자동 동기화)
     */
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
                val errorMessage = parseErrorMessage(errorString, response.code(), "포인트 차감 실패")
                Result.failure(Exception(errorMessage))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류가 발생했습니다: ${e.localizedMessage}"))
        }
    }

    private fun parseErrorMessage(
        errorString: String,
        statusCode: Int,
        defaultTitle: String = "요청 실패"
    ): String {
        return try {
            val match = Regex("\"detail\"\\s*:\\s*\"([^\"]+)\"").find(errorString)
            match?.groupValues?.get(1) ?: "$defaultTitle ($statusCode)"
        } catch (_: Exception) {
            errorString.ifBlank { "$defaultTitle ($statusCode)" }
        }
    }

    fun connectKioskWebSocket(binId: Int) {
        webSocketManager.connect(binId)
    }

    fun disconnectKioskWebSocket() {
        webSocketManager.disconnect()
    }
}
