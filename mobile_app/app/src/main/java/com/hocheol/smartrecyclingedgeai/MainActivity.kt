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

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    private val homeViewModel: HomeViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()

        // 딥링크 Intent 1회만 수신
        handleDeeplinkIntent(intent)

        setContent {
            SmartRecyclingEdgeAITheme {
                val loginViewModel: LoginViewModel = hiltViewModel()
                val historyViewModel: HistoryViewModel = hiltViewModel()
                val shopViewModel: ShopViewModel = hiltViewModel()
                val myPageViewModel: MyPageViewModel = hiltViewModel()

                val loginUiState by loginViewModel.uiState.collectAsStateWithLifecycle()
                val homeUiState by homeViewModel.uiState.collectAsStateWithLifecycle()
                val historyUiState by historyViewModel.uiState.collectAsStateWithLifecycle()
                val shopUiState by shopViewModel.uiState.collectAsStateWithLifecycle()
                val myPageUiState by myPageViewModel.uiState.collectAsStateWithLifecycle()

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
                        MainScreen(
                            homeUiState = homeUiState,
                            historyUiState = historyUiState,
                            shopUiState = shopUiState,
                            myPageUiState = myPageUiState,
                            onRefreshHome = { homeViewModel.refresh() },
                            onOpenQRScanner = { homeViewModel.openQRScanner() },
                            onCloseQRScanner = { homeViewModel.closeQRScanner() },
                            onQrScanned = { rawContent ->
                                homeViewModel.handleScannedQrContent(rawContent)
                            },
                            onConfirmResult = {
                                homeViewModel.dismissRecycleResultDialog()
                                historyViewModel.refresh()
                                shopViewModel.refresh()
                                myPageViewModel.loadMyPageData()
                            },
                            onCancelActiveSession = { homeViewModel.cancelKioskSession() },
                            onRefreshHistory = { historyViewModel.refresh() },
                            onCategorySelected = { category -> shopViewModel.selectCategory(category) },
                            onOpenPurchaseDialog = { product ->
                                shopViewModel.openPurchaseDialog(
                                    product
                                )
                            },
                            onDismissPurchaseDialog = { shopViewModel.dismissPurchaseDialog() },
                            onConfirmPurchase = {
                                shopViewModel.confirmPurchase()
                                homeViewModel.refresh()
                            },
                            onDismissCouponDialog = { shopViewModel.dismissCouponDialog() },
                            onShowLogoutDialog = { myPageViewModel.showLogoutDialog() },
                            onDismissLogoutDialog = { myPageViewModel.dismissLogoutDialog() },
                            onConfirmLogout = {
                                myPageViewModel.dismissLogoutDialog()
                                loginViewModel.logout()
                            },
                            onLogoutClick = { loginViewModel.logout() },
                            onErrorMessageShownHome = { homeViewModel.clearErrorMessage() },
                            onErrorMessageShownHistory = { historyViewModel.clearErrorMessage() },
                            onErrorMessageShownShop = { shopViewModel.clearErrorMessage() },
                            onErrorMessageShownMyPage = { myPageViewModel.clearErrorMessage() }
                        )
                    }

                    else -> {
                        val displayErrorMessage =
                            loginUiState.errorMessage ?: homeUiState.errorMessage

                        LoginScreen(
                            uiState = loginUiState.copy(errorMessage = displayErrorMessage),
                            onPhoneChanged = { loginViewModel.onPhoneChanged(it) },
                            onNameChanged = { loginViewModel.onNameChanged(it) },
                            onLoginClick = { loginViewModel.login() },
                            onErrorMessageShown = {
                                loginViewModel.clearErrorMessage()
                                homeViewModel.clearErrorMessage()
                            }
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

    private fun handleDeeplinkIntent(intent: Intent?) {
        val uri = intent?.data ?: return
        homeViewModel.handleDeeplink(uri)
        // 딥링크 중복 호출 방지를 위한 Intent Data 소진(Consume)
        intent.data = null
    }
}
