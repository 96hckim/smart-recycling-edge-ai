package com.hocheol.smartrecyclingedgeai.data.remote

import com.hocheol.smartrecyclingedgeai.data.model.request.LoginRequest
import com.hocheol.smartrecyclingedgeai.data.model.response.UserResponse
import com.hocheol.smartrecyclingedgeai.utils.Constants
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.POST

/**
 * 인증 및 사용자 로그인 관련 Retrofit2 REST API 서비스
 */
interface AuthApiService {
    @POST(Constants.ENDPOINT_LOGIN)
    suspend fun login(
        @Body request: LoginRequest
    ): Response<UserResponse>
}
