package com.hocheol.smartrecyclingedgeai.data.remote

import com.hocheol.smartrecyclingedgeai.data.model.request.KioskBindRequest
import com.hocheol.smartrecyclingedgeai.data.model.response.KioskBindResponse
import com.hocheol.smartrecyclingedgeai.data.model.response.RecycleLogListResponse
import com.hocheol.smartrecyclingedgeai.data.model.response.UserResponse
import com.hocheol.smartrecyclingedgeai.utils.Constants
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Path

interface KioskApiService {
    @GET(Constants.ENDPOINT_GET_USER)
    suspend fun getUserInfo(
        @Path("user_id") userId: Int
    ): Response<UserResponse>

    @POST(Constants.ENDPOINT_BIND_KIOSK)
    suspend fun bindKiosk(
        @Body request: KioskBindRequest
    ): Response<KioskBindResponse>

    @GET(Constants.ENDPOINT_GET_LOGS)
    suspend fun getUserLogs(
        @Path("user_id") userId: Int
    ): Response<RecycleLogListResponse>
}
