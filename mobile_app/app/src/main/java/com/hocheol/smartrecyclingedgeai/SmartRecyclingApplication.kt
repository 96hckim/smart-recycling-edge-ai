package com.hocheol.smartrecyclingedgeai

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

/**
 * 앱 전역 애플리케이션 클래스
 * [HiltAndroidApp] 어노테이션을 통해 Dagger-Hilt의 의존성 주입(DI) 그래프 컴포넌트를 초기화합니다.
 */
@HiltAndroidApp
class SmartRecyclingApplication : Application()
