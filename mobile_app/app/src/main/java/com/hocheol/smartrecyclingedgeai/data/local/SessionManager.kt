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

    val userPointsFlow: Flow<Int?> = context.dataStore.data.map { preferences ->
        preferences[KEY_POINTS]
    }

    val isLoggedInFlow: Flow<Boolean> = context.dataStore.data.map { preferences ->
        preferences[KEY_USER_ID] != null
    }

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

    suspend fun updatePoints(newPoints: Int) {
        context.dataStore.edit { preferences ->
            preferences[KEY_POINTS] = newPoints
        }
    }

    suspend fun isLoggedIn(): Boolean {
        val prefs = context.dataStore.data.first()
        return prefs[KEY_USER_ID] != null
    }

    suspend fun clearSession() {
        context.dataStore.edit { preferences ->
            preferences.clear()
        }
    }
}
