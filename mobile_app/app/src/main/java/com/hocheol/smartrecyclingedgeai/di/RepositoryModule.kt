package com.hocheol.smartrecyclingedgeai.di

import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.remote.AuthApiService
import com.hocheol.smartrecyclingedgeai.data.remote.KioskApiService
import com.hocheol.smartrecyclingedgeai.data.remote.KioskWebSocketManager
import com.hocheol.smartrecyclingedgeai.data.repository.AuthRepository
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

/**
 * 데이터 계층 리포지토리 관련 Hilt 싱글톤 DI 모듈
 */
@Module
@InstallIn(SingletonComponent::class)
object RepositoryModule {

    @Provides
    @Singleton
    fun provideAuthRepository(
        apiService: AuthApiService,
        sessionManager: SessionManager
    ): AuthRepository {
        return AuthRepository(apiService, sessionManager)
    }

    @Provides
    @Singleton
    fun provideKioskRepository(
        apiService: KioskApiService,
        webSocketManager: KioskWebSocketManager,
        sessionManager: SessionManager
    ): KioskRepository {
        return KioskRepository(apiService, webSocketManager, sessionManager)
    }
}
