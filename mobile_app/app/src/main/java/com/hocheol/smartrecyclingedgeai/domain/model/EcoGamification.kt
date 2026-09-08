package com.hocheol.smartrecyclingedgeai.domain.model

import com.hocheol.smartrecyclingedgeai.utils.Constants

/**
 * 사용자 배출 누적 횟수 기반 친환경 등급
 */
enum class EcoLevel(
    val levelName: String,
    val emoji: String,
    val description: String
) {
    SPROUT("새싹", "🌱", "지구를 위한 위대한 첫걸음!"),
    YOUNG_TREE("어린 나무", "🌿", "꾸준한 배출로 싱그러운 나뭇잎이 피어나요!"),
    LUSH_FOREST("울창한 숲", "🌳", "지구를 구하는 에코 히어로! 푸른 숲을 이루었어요!");

    companion object {
        fun fromCount(count: Int): EcoLevel {
            return when {
                count <= Constants.ECO_LEVEL1_MAX_COUNT -> SPROUT
                count <= Constants.ECO_LEVEL2_MAX_COUNT -> YOUNG_TREE
                else -> LUSH_FOREST
            }
        }
    }
}

/**
 * CO2 절감량 기반 소나무 심은 효과 환산 연산기
 */
object EcoCalculator {
    fun calculatePineTrees(carbonSavedG: Double): Double {
        if (carbonSavedG <= 0.0) return 0.0
        return carbonSavedG / Constants.CARBON_G_PER_PINE_TREE
    }
}
