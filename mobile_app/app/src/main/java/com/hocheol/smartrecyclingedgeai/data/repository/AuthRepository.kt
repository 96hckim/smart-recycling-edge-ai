package com.hocheol.smartrecyclingedgeai.data.repository

import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.model.request.LoginRequest
import com.hocheol.smartrecyclingedgeai.data.remote.AuthApiService
import com.hocheol.smartrecyclingedgeai.domain.model.User
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class AuthRepository @Inject constructor(
    private val apiService: AuthApiService,
    private val sessionManager: SessionManager
) {
    val userNameFlow: Flow<String?> = sessionManager.userNameFlow
    val phoneFlow: Flow<String?> = sessionManager.phoneFlow
    val isLoggedInFlow: Flow<Boolean> = sessionManager.isLoggedInFlow

    suspend fun login(phone: String, name: String?): Result<User> {
        return try {
            val request = LoginRequest(
                phone = phone,
                name = name?.ifBlank { null }
            )
            val response = apiService.login(request)
            val body = response.body()
            if (response.isSuccessful && body != null) {
                val user = User(
                    id = body.id,
                    phone = body.phone ?: "",
                    name = body.name ?: "회원",
                    points = body.points ?: 0,
                    createdAt = body.createdAt ?: ""
                )
                sessionManager.saveSession(
                    userId = user.id,
                    userName = user.name,
                    phone = user.phone
                )
                Result.success(user)
            } else {
                val errorMsg = response.errorBody()?.string() ?: "로그인에 실패했습니다."
                Result.failure(Exception("로그인 실패 (${response.code()}): $errorMsg"))
            }
        } catch (e: Exception) {
            Result.failure(Exception("네트워크 통신 오류가 발생했습니다: ${e.localizedMessage}"))
        }
    }

    suspend fun isLoggedIn(): Boolean = sessionManager.isLoggedIn()

    suspend fun clearSession() = sessionManager.clearSession()
}
