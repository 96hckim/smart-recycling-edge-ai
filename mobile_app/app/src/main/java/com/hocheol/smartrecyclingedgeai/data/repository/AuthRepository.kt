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
                    phone = user.phone
                )
                Result.success(user)
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
            val match = Regex("\"detail\"\\s*:\\s*\"([^\"]+)\"").find(errorString)
            match?.groupValues?.get(1) ?: "로그인 실패 ($statusCode)"
        } catch (_: Exception) {
            errorString.ifBlank { "로그인 실패 ($statusCode)" }
        }
    }

    suspend fun isLoggedIn(): Boolean = sessionManager.isLoggedIn()

    suspend fun clearSession() = sessionManager.clearSession()
}
