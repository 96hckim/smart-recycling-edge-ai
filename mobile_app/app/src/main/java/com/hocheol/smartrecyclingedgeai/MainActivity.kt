package com.hocheol.smartrecyclingedgeai

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.hocheol.smartrecyclingedgeai.presentation.home.HomeScreen
import com.hocheol.smartrecyclingedgeai.presentation.home.HomeViewModel
import com.hocheol.smartrecyclingedgeai.presentation.login.LoginScreen
import com.hocheol.smartrecyclingedgeai.presentation.login.LoginViewModel
import com.hocheol.smartrecyclingedgeai.ui.theme.SmartRecyclingEdgeAITheme

class MainActivity : ComponentActivity() {

    private val loginViewModel: LoginViewModel by viewModels {
        LoginViewModel.Factory(application)
    }

    private val homeViewModel: HomeViewModel by viewModels {
        HomeViewModel.Factory(application)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Edge-to-Edge 활성화 (투명 시스템바 및 라이트/다크 아이콘 스타일 제어는 SmartRecyclingEdgeAITheme에서 일괄 처리)
        enableEdgeToEdge()

        intent?.data?.let { uri ->
            homeViewModel.handleDeeplink(uri)
        }

        setContent {
            SmartRecyclingEdgeAITheme {
                val loginUiState by loginViewModel.uiState.collectAsState()
                val homeUiState by homeViewModel.uiState.collectAsState()

                when {
                    loginUiState.isCheckingAutoLogin -> {
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .background(MaterialTheme.colorScheme.background),
                            contentAlignment = Alignment.Center
                        ) {
                            CircularProgressIndicator(
                                color = MaterialTheme.colorScheme.primary,
                                strokeWidth = 3.dp
                            )
                        }
                    }
                    loginUiState.isLoggedIn -> {
                        HomeScreen(
                            uiState = homeUiState,
                            onRefresh = { homeViewModel.refresh() },
                            onOpenQRScanner = { homeViewModel.openQRScanner() },
                            onCloseQRScanner = { homeViewModel.closeQRScanner() },
                            onQrScanned = { rawContent -> homeViewModel.handleScannedQrContent(rawContent) },
                            onConfirmResult = { homeViewModel.dismissRecycleResultDialog() },
                            onCancelActiveSession = { homeViewModel.cancelKioskSession() },
                            onLogoutClick = { loginViewModel.logout() },
                            onErrorMessageShown = { homeViewModel.clearErrorMessage() }
                        )
                    }
                    else -> {
                        LoginScreen(
                            uiState = loginUiState,
                            onPhoneChanged = { loginViewModel.onPhoneChanged(it) },
                            onNameChanged = { loginViewModel.onNameChanged(it) },
                            onLoginClick = { loginViewModel.login() },
                            onErrorMessageShown = { loginViewModel.clearErrorMessage() }
                        )
                    }
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        intent.data?.let { uri ->
            homeViewModel.handleDeeplink(uri)
        }
    }
}
