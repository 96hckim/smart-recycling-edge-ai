package com.hocheol.smartrecyclingedgeai.utils

object Constants {
    // Network Base URLs
    const val BASE_URL = "http://100.72.78.11:8000/"
    const val WS_BASE_URL = "ws://100.72.78.11:8000/"

    // API Endpoints
    const val ENDPOINT_LOGIN = "api/auth/login"
    const val ENDPOINT_GET_USER = "api/users/{user_id}"
    const val ENDPOINT_BIND_KIOSK = "api/kiosk/bind"
    const val ENDPOINT_GET_LOGS = "api/users/{user_id}/logs"

    // Eco Gamification Factors
    const val CARBON_G_PER_PINE_TREE = 6600.0
    const val ECO_LEVEL1_MAX_COUNT = 5
    const val ECO_LEVEL2_MAX_COUNT = 15

    // Deeplink Specs
    const val DEEPLINK_SCHEME = "smartrecycle"
    const val DEEPLINK_HOST = "kiosk"
    const val PARAM_BIN_ID = "bin_id"

    // WebSocket Event Names
    const val EVENT_RECYCLE_COMPLETE = "RECYCLE_COMPLETE"

    // Network Timeouts
    const val CONNECT_TIMEOUT_SECONDS = 10L
    const val READ_TIMEOUT_SECONDS = 10L
    const val WRITE_TIMEOUT_SECONDS = 10L
}
