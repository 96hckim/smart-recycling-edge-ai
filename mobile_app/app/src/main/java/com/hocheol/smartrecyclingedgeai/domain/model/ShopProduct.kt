package com.hocheol.smartrecyclingedgeai.domain.model

enum class ShopCategory(val label: String, val emoji: String) {
    ALL("전체", "✨"),
    GIFTICON("기프티콘", "☕"),
    TRASH_BAG("종량제 봉투", "🗑️"),
    DONATION("환경 기부", "🌳")
}

data class ShopProduct(
    val id: Int,
    val name: String,
    val brand: String,
    val requiredPoints: Int,
    val category: ShopCategory,
    val emoji: String,
    val description: String
)

object DummyShopProducts {
    val sampleProducts = listOf(
        ShopProduct(
            id = 101,
            name = "아이스 아메리카노 Tall",
            brand = "스타벅스",
            requiredPoints = 4500,
            category = ShopCategory.GIFTICON,
            emoji = "☕",
            description = "시원하고 깔끔한 스타벅스 대표 에스프레소 음료"
        ),
        ShopProduct(
            id = 102,
            name = "재활용 쓰레기 종량제 봉투 20L (10장)",
            brand = "지자체 공식",
            requiredPoints = 3000,
            category = ShopCategory.TRASH_BAG,
            emoji = "🗑️",
            description = "친환경 소재로 제작된 규격 20리터 일반 쓰레기 봉투"
        ),
        ShopProduct(
            id = 103,
            name = "지구 살리기 나무 1그루 기부",
            brand = "에코 재단",
            requiredPoints = 6600,
            category = ShopCategory.DONATION,
            emoji = "🌳",
            description = "탄소 중립을 위해 도심 숲에 내 이름으로 나무 한 그루를 심습니다."
        ),
        ShopProduct(
            id = 104,
            name = "모바일 금액권 3,000원",
            brand = "CU 편의점",
            requiredPoints = 3000,
            category = ShopCategory.GIFTICON,
            emoji = "🏪",
            description = "전국 CU 편의점에서 자유롭게 사용 가능한 금액권"
        ),
        ShopProduct(
            id = 105,
            name = "음료 모바일 교환권 2,000원",
            brand = "메가MGC커피",
            requiredPoints = 2000,
            category = ShopCategory.GIFTICON,
            emoji = "🥤",
            description = "다양한 음료와 디저트를 즐길 수 있는 모바일 쿠폰"
        ),
        ShopProduct(
            id = 106,
            name = "해양 쓰레기 정화 모금 기부",
            brand = "그린피스",
            requiredPoints = 5000,
            category = ShopCategory.DONATION,
            emoji = "🌊",
            description = "바다 플라스틱 수거 및 해양 생태계 보호 활동 후원"
        )
    )
}
