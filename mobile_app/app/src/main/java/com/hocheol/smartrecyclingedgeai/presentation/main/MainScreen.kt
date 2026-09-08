package com.hocheol.smartrecyclingedgeai.presentation.main

import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.ShoppingCart
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.hocheol.smartrecyclingedgeai.R
import com.hocheol.smartrecyclingedgeai.domain.model.ShopCategory
import com.hocheol.smartrecyclingedgeai.domain.model.ShopProduct
import com.hocheol.smartrecyclingedgeai.presentation.history.HistoryScreen
import com.hocheol.smartrecyclingedgeai.presentation.history.HistoryUiState
import com.hocheol.smartrecyclingedgeai.presentation.home.HomeScreen
import com.hocheol.smartrecyclingedgeai.presentation.home.HomeUiState
import com.hocheol.smartrecyclingedgeai.presentation.mypage.MyPageScreen
import com.hocheol.smartrecyclingedgeai.presentation.mypage.MyPageUiState
import com.hocheol.smartrecyclingedgeai.presentation.shop.ShopScreen
import com.hocheol.smartrecyclingedgeai.presentation.shop.ShopUiState

/**
 * 앱의 메인 하단 내비게이션 바 컴포저블 컨테이너
 * 홈, 배출 이력, 상점, 마이페이지 4개 탭을 전환하며 인셋 패딩 조화를 관리합니다.
 */
@Composable
fun MainScreen(
    homeUiState: HomeUiState,
    historyUiState: HistoryUiState,
    shopUiState: ShopUiState,
    myPageUiState: MyPageUiState,
    onRefreshHome: () -> Unit,
    onOpenQRScanner: () -> Unit,
    onCloseQRScanner: () -> Unit,
    onQrScanned: (String) -> Unit,
    onConfirmResult: () -> Unit,
    onCancelActiveSession: () -> Unit,
    onRefreshHistory: () -> Unit,
    onCategorySelected: (ShopCategory) -> Unit,
    onOpenPurchaseDialog: (ShopProduct) -> Unit,
    onDismissPurchaseDialog: () -> Unit,
    onConfirmPurchase: () -> Unit,
    onDismissCouponDialog: () -> Unit,
    onShowLogoutDialog: () -> Unit,
    onDismissLogoutDialog: () -> Unit,
    onConfirmLogout: () -> Unit,
    onLogoutClick: () -> Unit,
    onErrorMessageShownHome: () -> Unit,
    onErrorMessageShownHistory: () -> Unit,
    onErrorMessageShownShop: () -> Unit,
    onErrorMessageShownMyPage: () -> Unit,
    modifier: Modifier = Modifier
) {
    var selectedTab by remember { mutableIntStateOf(0) }

    Scaffold(
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.surface,
                contentColor = MaterialTheme.colorScheme.onSurface
            ) {
                NavigationBarItem(
                    selected = selectedTab == 0,
                    onClick = {
                        selectedTab = 0
                    },
                    icon = {
                        Icon(
                            Icons.Default.Home,
                            contentDescription = stringResource(R.string.nav_home)
                        )
                    },
                    label = {
                        Text(
                            text = stringResource(R.string.nav_home),
                            fontSize = 12.sp,
                            fontWeight = if (selectedTab == 0) FontWeight.Bold else FontWeight.Medium
                        )
                    },
                    colors = NavigationBarItemDefaults.colors(
                        selectedIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                        selectedTextColor = MaterialTheme.colorScheme.primary,
                        unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        indicatorColor = MaterialTheme.colorScheme.primaryContainer
                    )
                )

                NavigationBarItem(
                    selected = selectedTab == 1,
                    onClick = {
                        selectedTab = 1
                    },
                    icon = {
                        Icon(
                            Icons.AutoMirrored.Filled.List,
                            contentDescription = stringResource(R.string.nav_history)
                        )
                    },
                    label = {
                        Text(
                            text = stringResource(R.string.nav_history),
                            fontSize = 12.sp,
                            fontWeight = if (selectedTab == 1) FontWeight.Bold else FontWeight.Medium
                        )
                    },
                    colors = NavigationBarItemDefaults.colors(
                        selectedIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                        selectedTextColor = MaterialTheme.colorScheme.primary,
                        unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        indicatorColor = MaterialTheme.colorScheme.primaryContainer
                    )
                )

                NavigationBarItem(
                    selected = selectedTab == 2,
                    onClick = {
                        selectedTab = 2
                    },
                    icon = {
                        Icon(
                            Icons.Default.ShoppingCart,
                            contentDescription = stringResource(R.string.nav_shop)
                        )
                    },
                    label = {
                        Text(
                            text = stringResource(R.string.nav_shop),
                            fontSize = 12.sp,
                            fontWeight = if (selectedTab == 2) FontWeight.Bold else FontWeight.Medium
                        )
                    },
                    colors = NavigationBarItemDefaults.colors(
                        selectedIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                        selectedTextColor = MaterialTheme.colorScheme.primary,
                        unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        indicatorColor = MaterialTheme.colorScheme.primaryContainer
                    )
                )

                NavigationBarItem(
                    selected = selectedTab == 3,
                    onClick = {
                        selectedTab = 3
                    },
                    icon = {
                        Icon(
                            Icons.Default.Person,
                            contentDescription = stringResource(R.string.nav_mypage)
                        )
                    },
                    label = {
                        Text(
                            text = stringResource(R.string.nav_mypage),
                            fontSize = 12.sp,
                            fontWeight = if (selectedTab == 3) FontWeight.Bold else FontWeight.Medium
                        )
                    },
                    colors = NavigationBarItemDefaults.colors(
                        selectedIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                        selectedTextColor = MaterialTheme.colorScheme.primary,
                        unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                        indicatorColor = MaterialTheme.colorScheme.primaryContainer
                    )
                )
            }
        },
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
        modifier = modifier
    ) { innerPadding ->
        // 내비게이션 바 위쪽 8dp 공통 여백으로 하위 스크린 시각적 밸런스 일률 유지
        val screenModifier = modifier
            .padding(innerPadding)
            .padding(bottom = 8.dp)

        when (selectedTab) {
            0 -> HomeScreen(
                uiState = homeUiState,
                onRefresh = onRefreshHome,
                onOpenQRScanner = onOpenQRScanner,
                onCloseQRScanner = onCloseQRScanner,
                onQrScanned = onQrScanned,
                onConfirmResult = onConfirmResult,
                onCancelActiveSession = onCancelActiveSession,
                onLogoutClick = onLogoutClick,
                onErrorMessageShown = onErrorMessageShownHome,
                modifier = screenModifier
            )

            1 -> HistoryScreen(
                uiState = historyUiState,
                onRefresh = onRefreshHistory,
                onErrorMessageShown = onErrorMessageShownHistory,
                modifier = screenModifier
            )

            2 -> ShopScreen(
                uiState = shopUiState,
                onCategorySelected = onCategorySelected,
                onOpenPurchaseDialog = onOpenPurchaseDialog,
                onDismissPurchaseDialog = onDismissPurchaseDialog,
                onConfirmPurchase = onConfirmPurchase,
                onDismissCouponDialog = onDismissCouponDialog,
                onErrorMessageShown = onErrorMessageShownShop,
                modifier = screenModifier
            )

            3 -> MyPageScreen(
                uiState = myPageUiState,
                onShowLogoutDialog = onShowLogoutDialog,
                onDismissLogoutDialog = onDismissLogoutDialog,
                onConfirmLogout = onConfirmLogout,
                onErrorMessageShown = onErrorMessageShownMyPage,
                modifier = screenModifier
            )
        }
    }
}
