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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.lifecycle.viewmodel.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.hocheol.smartrecyclingedgeai.presentation.history.HistoryViewModel
import com.hocheol.smartrecyclingedgeai.presentation.home.HomeViewModel
import com.hocheol.smartrecyclingedgeai.presentation.login.LoginScreen
import com.hocheol.smartrecyclingedgeai.presentation.login.LoginViewModel
import com.hocheol.smartrecyclingedgeai.presentation.main.MainScreen
import com.hocheol.smartrecyclingedgeai.presentation.mypage.MyPageViewModel
import com.hocheol.smartrecyclingedgeai.presentation.shop.ShopViewModel
import com.hocheol.smartrecyclingedgeai.ui.theme.SmartRecyclingEdgeAITheme
import dagger.hilt.android.AndroidEntryPoint

/**
 * 스마트 재활용수거 앱의 단일 액티비티(Single Activity)
 * 딥링크 진입, 세션 기반 화면 분기 및 ViewModel 의존성을 바인딩합니다.
 */
@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    // Activity 수명주기와 연동되는 HomeViewModel (딥링크 수신 및 세션 관리)
    private val homeViewModel: HomeViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()

        // 앱 실행 시 딥링크(QR 스캔 진입) 1회성 수신 처리
        handleDeeplinkIntent(intent)

        setContent {
            SmartRecyclingEdgeAITheme {
                val loginViewModel: LoginViewModel = hiltViewModel()
                val historyViewModel: HistoryViewModel = hiltViewModel()
                val shopViewModel: ShopViewModel = hiltViewModel()
                val myPageViewModel: MyPageViewModel = hiltViewModel()

                // 생명주기 안전 상태 수집 (백그라운드 리소스 고갈 방지)
                val loginUiState by loginViewModel.uiState.collectAsStateWithLifecycle()
                val homeUiState by homeViewModel.uiState.collectAsStateWithLifecycle()
                val historyUiState by historyViewModel.uiState.collectAsStateWithLifecycle()
                val shopUiState by shopViewModel.uiState.collectAsStateWithLifecycle()
                val myPageUiState by myPageViewModel.uiState.collectAsStateWithLifecycle()

                when {
                    // 1. 자동 로그인 체크 중 로딩 상태
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

                    // 2. 로그인 완료: 메인 화면 표시 및 이벤트 핸들러 바인딩
                    loginUiState.isLoggedIn -> {
                        val onConfirmResult = remember(
                            homeViewModel,
                            historyViewModel,
                            shopViewModel,
                            myPageViewModel
                        ) {
                            {
                                homeViewModel.dismissRecycleResultDialog()
                                historyViewModel.refresh()
                                shopViewModel.refresh()
                                myPageViewModel.loadMyPageData()
                            }
                        }
                        val onConfirmPurchase = remember(shopViewModel, homeViewModel) {
                            {
                                shopViewModel.confirmPurchase()
                                homeViewModel.refresh()
                            }
                        }
                        val onConfirmLogout = remember(myPageViewModel, loginViewModel) {
                            {
                                myPageViewModel.dismissLogoutDialog()
                                loginViewModel.logout()
                            }
                        }

                        MainScreen(
                            homeUiState = homeUiState,
                            historyUiState = historyUiState,
                            shopUiState = shopUiState,
                            myPageUiState = myPageUiState,
                            onRefreshHome = homeViewModel::refresh,
                            onOpenQRScanner = homeViewModel::openQRScanner,
                            onCloseQRScanner = homeViewModel::closeQRScanner,
                            onQrScanned = homeViewModel::handleScannedQrContent,
                            onConfirmResult = onConfirmResult,
                            onCancelActiveSession = homeViewModel::cancelKioskSession,
                            onRefreshHistory = historyViewModel::refresh,
                            onCategorySelected = shopViewModel::selectCategory,
                            onOpenPurchaseDialog = shopViewModel::openPurchaseDialog,
                            onDismissPurchaseDialog = shopViewModel::dismissPurchaseDialog,
                            onConfirmPurchase = onConfirmPurchase,
                            onDismissCouponDialog = shopViewModel::dismissCouponDialog,
                            onShowLogoutDialog = myPageViewModel::showLogoutDialog,
                            onDismissLogoutDialog = myPageViewModel::dismissLogoutDialog,
                            onConfirmLogout = onConfirmLogout,
                            onLogoutClick = loginViewModel::logout,
                            onErrorMessageShownHome = homeViewModel::clearErrorMessage,
                            onErrorMessageShownHistory = historyViewModel::clearErrorMessage,
                            onErrorMessageShownShop = shopViewModel::clearErrorMessage,
                            onErrorMessageShownMyPage = myPageViewModel::clearErrorMessage
                        )
                    }

                    // 3. 비로그인 상태: 로그인 화면 및 외부 딥링크 메세지 수신
                    else -> {
                        val displayErrorMessage =
                            loginUiState.errorMessage ?: homeUiState.errorMessage

                        val onLoginErrorMessageShown = remember(loginViewModel, homeViewModel) {
                            {
                                loginViewModel.clearErrorMessage()
                                homeViewModel.clearErrorMessage()
                            }
                        }

                        LoginScreen(
                            uiState = loginUiState.copy(errorMessage = displayErrorMessage),
                            onPhoneChanged = loginViewModel::onPhoneChanged,
                            onNameChanged = loginViewModel::onNameChanged,
                            onLoginClick = loginViewModel::login,
                            onErrorMessageShown = onLoginErrorMessageShown
                        )
                    }
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleDeeplinkIntent(intent)
    }

    /**
     * 외부 QR 카메라 및 딥링크 진입 처리
     * 딥링크 중복 및 무한 Recomposition 방지를 위해 Intent data를 1회 처리 후 소진(Consume)합니다.
     */
    private fun handleDeeplinkIntent(intent: Intent?) {
        val uri = intent?.data ?: return
        homeViewModel.handleDeeplink(uri)
        intent.data = null
    }
}
