package com.hocheol.smartrecyclingedgeai.utils

/**
 * 개인정보 보호를 위한 휴대폰 번호 마스킹 유틸리티 (ex: 010-****-5678)
 */
object PhoneUtils {
    fun maskPhoneNumber(phone: String?): String {
        if (phone.isNullOrBlank()) return ""
        val cleanPhone = phone.filter { it.isDigit() }
        return when (cleanPhone.length) {
            11 -> {
                "${cleanPhone.substring(0, 3)}-****-${cleanPhone.substring(7)}"
            }

            10 -> {
                "${cleanPhone.substring(0, 3)}-***-${cleanPhone.substring(6)}"
            }

            else -> phone
        }
    }
}
