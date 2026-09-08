package com.hocheol.smartrecyclingedgeai.data.local

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

private val Context.dataStore by preferencesDataStore(name = "user_session")

/**
 * DataStore Preferences 기반 세션 관리자 (Single Source of Truth)
 * 로그인 유저 세션 및 잔여 포인트 데이터를 비동기 안전 스트림(Flow)으로 제공합니다.
 */
@Singleton
class SessionManager @Inject constructor(
    @ApplicationContext private val context: Context
) {
    companion object {
        private val KEY_USER_ID = intPreferencesKey("user_id")
        private val KEY_USER_NAME = stringPreferencesKey("user_name")
        private val KEY_PHONE = stringPreferencesKey("phone")
        private val KEY_POINTS = intPreferencesKey("user_points")
    }

    val userIdFlow: Flow<Int?> = context.dataStore.data.map { preferences ->
        preferences[KEY_USER_ID]
    }

    val userNameFlow: Flow<String?> = context.dataStore.data.map { preferences ->
        preferences[KEY_USER_NAME]
    }

    val phoneFlow: Flow<String?> = context.dataStore.data.map { preferences ->
        preferences[KEY_PHONE]
    }

    // 전 탭 포인트 실시간 반응형 동기화를 위한 StateFlow 원천
    val userPointsFlow: Flow<Int?> = context.dataStore.data.map { preferences ->
        preferences[KEY_POINTS]
    }

    val isLoggedInFlow: Flow<Boolean> = context.dataStore.data.map { preferences ->
        preferences[KEY_USER_ID] != null
    }

    /**
     * 로그인 성공 시 유저 정보 및 포인트를 영속성 저장소에 저장
     */
    suspend fun saveSession(userId: Int, userName: String, phone: String, points: Int? = null) {
        context.dataStore.edit { preferences ->
            preferences[KEY_USER_ID] = userId
            preferences[KEY_USER_NAME] = userName
            preferences[KEY_PHONE] = phone
            if (points != null) {
                preferences[KEY_POINTS] = points
            }
        }
    }

    /**
     * 적립/차감 등으로 변경된 잔여 포인트를 세션에 갱신하여 앱 전반에 자동 전파
     */
    suspend fun updatePoints(newPoints: Int) {
        context.dataStore.edit { preferences ->
            preferences[KEY_POINTS] = newPoints
        }
    }

    suspend fun isLoggedIn(): Boolean {
        val prefs = context.dataStore.data.first()
        return prefs[KEY_USER_ID] != null
    }

    /**
     * 로그아웃 시 저장된 모든 사용자 세션 초기화
     */
    suspend fun clearSession() {
        context.dataStore.edit { preferences ->
            preferences.clear()
        }
    }
}
