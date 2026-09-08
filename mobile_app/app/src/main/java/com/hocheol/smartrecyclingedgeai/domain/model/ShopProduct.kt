package com.hocheol.smartrecyclingedgeai.domain.model

import androidx.compose.runtime.Immutable

/**
 * 포인트 상점 카테고리
 */
enum class ShopCategory(val label: String, val emoji: String) {
    ALL("전체", "✨"),
    GIFTICON("기프티콘", "☕"),
    TRASH_BAG("종량제 봉투", "🗑️"),
    DONATION("환경 기부", "🌳")
}

/**
 * 포인트 상점 교환 가능 상품 도메인 모델
 */
@Immutable
data class ShopProduct(
    val id: Int,
    val name: String,
    val brand: String,
    val requiredPoints: Int,
    val category: ShopCategory,
    val emoji: String,
    val description: String
)
