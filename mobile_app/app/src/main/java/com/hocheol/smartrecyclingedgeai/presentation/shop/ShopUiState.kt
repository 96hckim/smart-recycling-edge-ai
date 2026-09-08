package com.hocheol.smartrecyclingedgeai.presentation.shop

import androidx.compose.runtime.Immutable
import com.hocheol.smartrecyclingedgeai.domain.model.ShopCategory
import com.hocheol.smartrecyclingedgeai.domain.model.ShopProduct

/**
 * 포인트 상점 화면 불변(Immutable) UI 상태
 */
@Immutable
data class ShopUiState(
    val userPoints: Int = 0,
    val selectedCategory: ShopCategory = ShopCategory.ALL,
    val products: List<ShopProduct> = emptyList(),
    val selectedProductForPurchase: ShopProduct? = null,
    val purchasedCouponCode: String? = null,
    val purchasedProduct: ShopProduct? = null,
    val isLoading: Boolean = false,
    val errorMessage: String? = null
)
