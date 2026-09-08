package com.hocheol.smartrecyclingedgeai.presentation.shop

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.datasource.FakeShopDataSource
import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import com.hocheol.smartrecyclingedgeai.domain.model.ShopCategory
import com.hocheol.smartrecyclingedgeai.domain.model.ShopProduct
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.firstOrNull
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.util.UUID
import javax.inject.Inject

/**
 * 포인트 상점(Eco Store) 뷰모델
 * 카테고리 필터링, 포인트 교환 백엔드 API 연동 및 쿠폰 발급 시뮬레이션을 관리합니다.
 */
@HiltViewModel
class ShopViewModel @Inject constructor(
    private val kioskRepository: KioskRepository,
    private val sessionManager: SessionManager
) : ViewModel() {

    private val _uiState = MutableStateFlow(ShopUiState())
    val uiState: StateFlow<ShopUiState> = _uiState.asStateFlow()

    init {
        loadProducts()
        observeUserPoints()
    }

    private fun loadProducts() {
        _uiState.update { it.copy(products = FakeShopDataSource.sampleProducts) }
    }

    private fun observeUserPoints() {
        viewModelScope.launch {
            sessionManager.userIdFlow.collectLatest { userId ->
                if (userId != null) {
                    fetchUserPoints(userId)
                } else {
                    _uiState.update { it.copy(userPoints = 0) }
                }
            }
        }
        viewModelScope.launch {
            sessionManager.userPointsFlow.collectLatest { points ->
                if (points != null) {
                    _uiState.update { it.copy(userPoints = points) }
                }
            }
        }
    }

    fun refresh() {
        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId != null) {
                fetchUserPoints(userId)
            }
        }
    }

    private suspend fun fetchUserPoints(userId: Int) {
        val userResult = kioskRepository.getUserInfo(userId)
        val points = userResult.getOrNull()?.points ?: 0
        _uiState.update { it.copy(userPoints = points) }
    }

    fun selectCategory(category: ShopCategory) {
        _uiState.update { it.copy(selectedCategory = category) }
    }

    fun openPurchaseDialog(product: ShopProduct) {
        _uiState.update { it.copy(selectedProductForPurchase = product) }
    }

    fun dismissPurchaseDialog() {
        _uiState.update { it.copy(selectedProductForPurchase = null) }
    }

    /**
     * 포인트 차감 백엔드 API(POST /api/users/deduct) 연동 및 기프티콘 발급 시뮬레이션
     */
    fun confirmPurchase() {
        val currentState = _uiState.value
        val product = currentState.selectedProductForPurchase ?: return

        _uiState.update { it.copy(isLoading = true, errorMessage = null) }

        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId == null) {
                _uiState.update { state ->
                    state.copy(
                        isLoading = false,
                        selectedProductForPurchase = null,
                        errorMessage = "상품 교환을 위해 로그인이 필요합니다."
                    )
                }
                return@launch
            }
            val description = "${product.brand} ${product.name}"

            val result = kioskRepository.deductPoints(
                userId = userId,
                amount = product.requiredPoints,
                description = description
            )

            result.onSuccess { response ->
                val couponCode = "ECO-2026-" + UUID.randomUUID().toString().take(8).uppercase()
                _uiState.update { state ->
                    state.copy(
                        isLoading = false,
                        userPoints = response.remainingPoints,
                        selectedProductForPurchase = null,
                        purchasedProduct = product,
                        purchasedCouponCode = couponCode
                    )
                }
            }.onFailure { exception ->
                _uiState.update { state ->
                    state.copy(
                        isLoading = false,
                        selectedProductForPurchase = null,
                        errorMessage = exception.message ?: "포인트 차감 요청 중 오류가 발생했습니다."
                    )
                }
            }
        }
    }

    fun dismissCouponDialog() {
        _uiState.update {
            it.copy(
                purchasedProduct = null,
                purchasedCouponCode = null
            )
        }
    }

    fun clearErrorMessage() {
        _uiState.update { it.copy(errorMessage = null) }
    }
}
