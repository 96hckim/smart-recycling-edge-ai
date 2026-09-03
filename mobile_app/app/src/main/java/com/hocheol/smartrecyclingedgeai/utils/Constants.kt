package com.hocheol.smartrecyclingedgeai.utils

object Constants {
    // Network Base URLs
    const val BASE_URL = "http://100.72.78.11:8000/"
    const val WS_BASE_URL = "ws://100.72.78.11:8000/"

    // API Endpoints
    const val ENDPOINT_LOGIN = "api/auth/login"
    const val ENDPOINT_GET_USER = "api/users/{user_id}"
    const val ENDPOINT_BIND_KIOSK = "api/kiosk/bind"

    // Deeplink Specs
    const val DEEPLINK_SCHEME = "smartrecycle"
    const val DEEPLINK_HOST = "kiosk"
    const val DEEPLINK_PATH_AUTH = "/auth"
    const val PARAM_BIN_ID = "bin_id"

    // WebSocket Event Names
    const val EVENT_RECYCLE_COMPLETE = "RECYCLE_COMPLETE"

    // Item Category Keys
    const val KEY_PAPER_COUNT = "paper_count"
    const val KEY_CAN_COUNT = "can_count"
    const val KEY_PET_COUNT = "pet_count"
    const val KEY_VINYL_COUNT = "vinyl_count"

    // Carbon Saved Factors (grams per item)
    const val CARBON_SAVED_PER_PAPER_G = 15.0
    const val CARBON_SAVED_PER_CAN_G = 30.0
    const val CARBON_SAVED_PER_PET_G = 20.0
    const val CARBON_SAVED_PER_VINYL_G = 10.0

    // Network Timeouts
    const val CONNECT_TIMEOUT_SECONDS = 10L
    const val READ_TIMEOUT_SECONDS = 10L
    const val WRITE_TIMEOUT_SECONDS = 10L
}
