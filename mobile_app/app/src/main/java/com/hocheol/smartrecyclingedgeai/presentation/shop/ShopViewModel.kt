package com.hocheol.smartrecyclingedgeai.presentation.shop

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hocheol.smartrecyclingedgeai.data.local.SessionManager
import com.hocheol.smartrecyclingedgeai.data.repository.KioskRepository
import com.hocheol.smartrecyclingedgeai.domain.model.DummyShopProducts
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
        _uiState.update { it.copy(products = DummyShopProducts.sampleProducts) }
    }

    private fun observeUserPoints() {
        viewModelScope.launch {
            sessionManager.userIdFlow.collectLatest { userId ->
                if (userId != null) {
                    val userResult = kioskRepository.getUserInfo(userId)
                    val points = userResult.getOrNull()?.points ?: 0
                    _uiState.update { it.copy(userPoints = points) }
                } else {
                    _uiState.update { it.copy(userPoints = 0) }
                }
            }
        }
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

    fun confirmPurchase() {
        val currentState = _uiState.value
        val product = currentState.selectedProductForPurchase ?: return

        if (currentState.userPoints < product.requiredPoints) {
            _uiState.update {
                it.copy(
                    selectedProductForPurchase = null,
                    errorMessage = "보유 포인트가 부족합니다."
                )
            }
            return
        }

        val newPoints = currentState.userPoints - product.requiredPoints
        val couponCode = "ECO-2026-" + UUID.randomUUID().toString().take(8).uppercase()

        _uiState.update {
            it.copy(
                userPoints = newPoints,
                selectedProductForPurchase = null,
                purchasedProduct = product,
                purchasedCouponCode = couponCode
            )
        }

        viewModelScope.launch {
            val userId = sessionManager.userIdFlow.firstOrNull()
            if (userId != null) {
                kioskRepository.getUserInfo(userId)
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
