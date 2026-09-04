package com.hocheol.smartrecyclingedgeai

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        enableEdgeToEdge()

        setContent {
            SmartRecyclingEdgeAITheme {
                val loginViewModel: LoginViewModel = hiltViewModel()
                val homeViewModel: HomeViewModel = hiltViewModel()
                val historyViewModel: HistoryViewModel = hiltViewModel()
                val shopViewModel: ShopViewModel = hiltViewModel()
                val myPageViewModel: MyPageViewModel = hiltViewModel()

                val loginUiState by loginViewModel.uiState.collectAsStateWithLifecycle()
                val homeUiState by homeViewModel.uiState.collectAsStateWithLifecycle()
                val historyUiState by historyViewModel.uiState.collectAsStateWithLifecycle()
                val shopUiState by shopViewModel.uiState.collectAsStateWithLifecycle()
                val myPageUiState by myPageViewModel.uiState.collectAsStateWithLifecycle()

                intent?.data?.let { uri ->
                    homeViewModel.handleDeeplink(uri)
                }

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
        // intent handling
    }
}
