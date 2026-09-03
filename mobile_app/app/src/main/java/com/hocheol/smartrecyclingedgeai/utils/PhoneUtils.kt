package com.hocheol.smartrecyclingedgeai.utils

object PhoneUtils {
    fun maskPhoneNumber(phone: String?): String {
        if (phone.isNullOrBlank()) return ""
        val cleanPhone = phone.filter { it.isDigit() }
        return when {
            cleanPhone.length == 11 -> {
                "${cleanPhone.substring(0, 3)}-****-${cleanPhone.substring(7)}"
            }
            cleanPhone.length == 10 -> {
                "${cleanPhone.substring(0, 3)}-***-${cleanPhone.substring(6)}"
            }
            else -> phone
        }
    }
}
