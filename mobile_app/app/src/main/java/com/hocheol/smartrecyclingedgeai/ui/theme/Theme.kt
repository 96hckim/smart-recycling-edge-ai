package com.hocheol.smartrecyclingedgeai.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val LightColorScheme = lightColorScheme(
    primary = ForestEmerald,
    onPrimary = Color.White,
    primaryContainer = ForestEmeraldLight,
    onPrimaryContainer = ForestEmeraldDark,

    secondary = SkyBlue,
    onSecondary = Color.White,
    secondaryContainer = SkyBlueLight,
    onSecondaryContainer = Color(0xFF0369A1),

    tertiary = AmberAccent,
    onTertiary = Color.White,
    tertiaryContainer = AmberAccentLight,
    onTertiaryContainer = Color(0xFFB45309),

    background = CleanNeutralBg,
    onBackground = TextDarkSlate,

    surface = SurfaceWhite,
    onSurface = TextDarkSlate,
    surfaceVariant = CleanNeutralBg,
    onSurfaceVariant = TextMutedGray,

    outline = TextMutedGray,
    outlineVariant = Color(0xFFE2E8F0),

    error = ErrorRed,
    onError = Color.White
)

private val DarkColorScheme = darkColorScheme(
    primary = ForestEmerald,
    onPrimary = Color.White,
    primaryContainer = Color(0xFF065F46),
    onPrimaryContainer = Color(0xFFA7F3D0),

    secondary = SkyBlue,
    onSecondary = Color.White,
    secondaryContainer = Color(0xFF075985),
    onSecondaryContainer = Color(0xFFBAE6FD),

    tertiary = AmberAccent,
    onTertiary = Color.White,
    tertiaryContainer = Color(0xFF78350F),
    onTertiaryContainer = Color(0xFFFDE68A),

    background = Color(0xFF0F172A),
    onBackground = Color(0xFFF8FAFC),

    surface = Color(0xFF1E293B),
    onSurface = Color(0xFFF8FAFC),
    surfaceVariant = Color(0xFF334155),
    onSurfaceVariant = Color(0xFFCBD5E1),

    outline = Color(0xFF64748B),
    error = ErrorRed,
    onError = Color.White
)

// 4대 분리수거 카테고리 뱃지 전용 디자인 시스템 색상 스펙
data class BadgeColorScheme(
    val paperBg: Color,
    val paperText: Color,
    val canBg: Color,
    val canText: Color,
    val petBg: Color,
    val petText: Color,
    val vinylBg: Color,
    val vinylText: Color
)

private val LightBadgeColorScheme = BadgeColorScheme(
    paperBg = Color(0xFFFEF3C7), paperText = Color(0xFFB45309),
    canBg = Color(0xFFD1FAE5), canText = Color(0xFF047857),
    petBg = Color(0xFFE0F2FE), petText = Color(0xFF0284C7),
    vinylBg = Color(0xFFF3E8FF), vinylText = Color(0xFF7E22CE)
)

private val DarkBadgeColorScheme = BadgeColorScheme(
    paperBg = Color(0xFF78350F), paperText = Color(0xFFFDE68A),
    canBg = Color(0xFF065F46), canText = Color(0xFFA7F3D0),
    petBg = Color(0xFF075985), petText = Color(0xFFBAE6FD),
    vinylBg = Color(0xFF581C87), vinylText = Color(0xFFE9D5FF)
)

val LocalBadgeColorScheme = staticCompositionLocalOf { LightBadgeColorScheme }

object AppTheme {
    val badgeColors: BadgeColorScheme
        @Composable
        @ReadOnlyComposable
        get() = LocalBadgeColorScheme.current
}

@Composable
fun SmartRecyclingEdgeAITheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val colorScheme = if (darkTheme) DarkColorScheme else LightColorScheme
    val badgeColorScheme = if (darkTheme) DarkBadgeColorScheme else LightBadgeColorScheme
    val view = LocalView.current

    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as? Activity)?.window
            if (window != null) {
                val insetsController = WindowCompat.getInsetsController(window, view)
                insetsController.isAppearanceLightStatusBars = !darkTheme
                insetsController.isAppearanceLightNavigationBars = !darkTheme
            }
        }
    }

    CompositionLocalProvider(
        LocalBadgeColorScheme provides badgeColorScheme
    ) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = Typography,
            content = content
        )
    }
}
