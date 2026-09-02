#pragma once
#ifndef THEME_CONSTANTS_H
#define THEME_CONSTANTS_H

#include <QString>

namespace UITheme {
constexpr char PROP_MEMBER[] = "member";
constexpr char PROP_BANNER_STATUS[] = "bannerStatus";

enum class BannerType {
    READY,
    ANALYZING,
    CONFIRMED,
    WARNING
};

namespace Text {
    constexpr char GREETING_MEMBER_FMT[] = "👤 %1 님 환영합니다";
    constexpr char GREETING_GUEST[] = "👤 비회원 간편 투입";
    constexpr char SESSION_MODE_MEMBER[] = "투입 완료 후 포인트가 자동으로 적립됩니다.";
    constexpr char SESSION_MODE_GUEST[] = "비회원 모드로 동작 중입니다 (포인트 미적립).";
    constexpr char REWARD_HEADER_MEMBER[] = "💰 현재 세션 획득 리워드";
    constexpr char REWARD_HEADER_GUEST[] = "⚠️ 비회원 미적립 혜택 안내"; // 헤더 타이틀 차별화

    constexpr char DETECT_WAITING[] = "물품 인식 대기 중...";
    constexpr char DETECT_CONFIDENCE_EMPTY[] = "신뢰도: - %";
    constexpr char DETECT_CONFIDENCE_FMT[] = "신뢰도: %1%";
    constexpr char DETECT_ITEM_FMT[] = "🔍 %1 감지됨";

    constexpr char GUIDE_READY[] = "🎯 카메라 중앙 영역에 재활용품을 놓아주세요";
    constexpr char GUIDE_ANALYZING_FMT[] = "⏳ %1 인식 중... 고정해 주세요";
    constexpr char GUIDE_CONFIRMED_FMT[] = "✅ %1 인식 확정! 투입구에 넣어주세요";
    constexpr char GUIDE_GENERAL_WARN[] = "⚠️ 일반쓰레기 감지 (포인트 미지급)";

    constexpr char POINTS_MEMBER_FMT[] = "+ %1 P";
    constexpr char POINTS_GUEST_FMT[] = "%1 P (미적립)"; // 비회원 잠재 포인트 표출 폼
    constexpr char CARBON_SAVED_FMT[] = "🌱 절감 탄소량: %1g CO2";
    constexpr char VIDEO_INITIALIZING[] = "Jetson AI 비전 스트림 연결 대기 중...";
}

struct BannerThemeDef {
    const char* textColor;
    const char* borderColor;
    const char* bgColor;
    const char* qssStatus;
};

inline BannerThemeDef getBannerTheme(BannerType type)
{
    switch (type) {
    case BannerType::READY:
        return { "#38BDF8", "#0284C7", "#0D1927", "ready" };
    case BannerType::ANALYZING:
        return { "#FBBF24", "#D97706", "rgba(245, 158, 11, 0.15)", "analyzing" };
    case BannerType::CONFIRMED:
        return { "#10B981", "#10B981", "rgba(16, 185, 129, 0.15)", "confirmed" };
    case BannerType::WARNING:
        return { "#F1F5F9", "#94A3B8", "rgba(148, 163, 184, 0.2)", "warning" };
    }
    return { "#FFFFFF", "#334155", "transparent", "default" };
}

namespace Style {
    constexpr char GREETING_MEMBER[] = "font-size: 30px; font-weight: 900; color: #10B981; background: transparent;";
    constexpr char GREETING_GUEST[] = "font-size: 30px; font-weight: 900; color: #38BDF8; background: transparent;";
    constexpr char POINTS_MEMBER[] = "font-size: 42px; font-weight: 900; color: #38BDF8; background: transparent;";
    constexpr char POINTS_GUEST[] = "font-size: 32px; font-weight: 800; color: #F59E0B; background: transparent;"; // 주황빛 경고 톤
    constexpr char BANNER_TEMPLATE[] = "background-color: %1; border: 2px solid %2; border-radius: 16px; color: %3; font-size: 22px; font-weight: 800; padding: 10px;";
}
}

#endif // THEME_CONSTANTS_H