#pragma once
#ifndef THEME_CONSTANTS_H
#define THEME_CONSTANTS_H

#include <QSize>
#include <QString>

namespace UITheme {

// ============================================================================
// 1. 동적 스타일(QSS) 프로퍼티 키
// ============================================================================
constexpr char PROP_MEMBER[] = "member";
constexpr char PROP_BANNER_STATUS[] = "bannerStatus";
inline constexpr const char* PROP_ACTIVE = "active";

// ============================================================================
// 2. 실시간 AI 가이드 배너 상태 정의
// ============================================================================
enum class BannerType {
    READY,
    ANALYZING,
    CONFIRMED,
    WARNING
};

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

// ============================================================================
// 3. 인라인 스타일시트 및 서식
// ============================================================================
namespace Style {
    inline const QString FONT_FAMILY = "Pretendard";
    inline const QString DEFAULT_BOX_COLOR = "#10B981";

    constexpr char GREETING_MEMBER[] = "font-size: 30px; font-weight: 900; color: #10B981; background: transparent;";
    constexpr char GREETING_GUEST[] = "font-size: 30px; font-weight: 900; color: #38BDF8; background: transparent;";
    constexpr char POINTS_MEMBER[] = "font-size: 42px; font-weight: 900; color: #38BDF8; background: transparent;";
    constexpr char POINTS_GUEST[] = "font-size: 32px; font-weight: 800; color: #F59E0B; background: transparent;";
    constexpr char BANNER_TEMPLATE[] = "background-color: %1; border: 2px solid %2; border-radius: 16px; color: %3; font-size: 22px; font-weight: 800; padding: 10px;";
}

// ============================================================================
// 4. 전역 UI 표출 문자열 및 포맷
// ============================================================================
namespace Text {
    // 단위 포맷
    inline const QString COUNT_UNIT_FMT = "%1 개";
    inline const QString POINTS_PLUS_FMT = "+ %1 P";
    inline const QString POINTS_ZERO = "0 P";
    inline const QString EMPTY_DASH = "-";

    // 사용자 세션 및 호칭
    inline const QString DEFAULT_MEMBER_NAME = "회원";
    constexpr char GREETING_MEMBER_FMT[] = "👤 %1 님 환영합니다";
    constexpr char GREETING_GUEST[] = "👤 비회원 간편 투입";
    constexpr char SESSION_MODE_MEMBER[] = "투입 완료 후 포인트가 자동으로 적립됩니다.";
    constexpr char SESSION_MODE_GUEST[] = "비회원 모드로 동작 중입니다 (포인트 미적립).";
    constexpr char REWARD_HEADER_MEMBER[] = "💰 현재 세션 획득 리워드";
    constexpr char REWARD_HEADER_GUEST[] = "⚠️ 비회원 미적립 혜택 안내";

    // AI 비전 감지 및 가이드 배너
    constexpr char VIDEO_INITIALIZING[] = "Jetson AI 비전 스트림 연결 대기 중...";
    constexpr char DETECT_WAITING[] = "물품 인식 대기 중...";
    constexpr char DETECT_CONFIDENCE_EMPTY[] = "신뢰도: - %";
    constexpr char DETECT_CONFIDENCE_FMT[] = "신뢰도: %1%";
    constexpr char DETECT_ITEM_FMT[] = "🔍 %1 감지됨";
    constexpr char GUIDE_READY[] = "🎯 카메라 중앙 영역에 재활용품을 놓아주세요";
    constexpr char GUIDE_ANALYZING_FMT[] = "⏳ %1 인식 중... 고정해 주세요";
    constexpr char GUIDE_CONFIRMED_FMT[] = "✅ %1 인식 확정! 투입구에 넣어주세요";
    constexpr char GUIDE_GENERAL_WARN[] = "⚠️ 일반쓰레기 감지 (포인트 미지급)";

    // 결과 화면
    inline const QString RESULT_NOTICE_MEMBER_FMT = "✅ %1 님의 계정으로 포인트가 안전하게 적립되었습니다.";
    inline const QString RESULT_NOTICE_GUEST = "ℹ️ 비회원 이용 세션 (모바일 앱 가입 후 스캔 시 포인트가 적립됩니다)";
    inline const QString RESULT_POINTS_GUEST = "0 P (비회원)";
    inline const QString RESULT_CARBON_FMT = "%1 g CO₂";
    inline const QString RESULT_COUNTDOWN_BTN_FMT = "확인 (%1초 후 처음으로 이동)";
    constexpr char POINTS_MEMBER_FMT[] = "+ %1 P";
    constexpr char POINTS_GUEST_FMT[] = "%1 P (미적립)";
    constexpr char CARBON_SAVED_FMT[] = "🌱 절감 탄소량: %1g CO2";
}

namespace Result {
    // 타이틀 글자 영역(약 300px)에 최적화된 16:9 비율 폭죽 해상도
    inline const QSize CONFETTI_DISPLAY_SIZE { 640, 300 }; // 1280, 720
}

// ============================================================================
// 5. 에코 트리 UI 리소스 및 문구
// ============================================================================
namespace EcoTree {
    inline const QSize DISPLAY_SIZE { 300, 300 };

    inline const QString STATUS_BASE = "재활용품을 넣어 나무에 잎을 틔워보세요!";
    inline const QString STATUS_STAGE_1_FMT = "🌱 연둣빛 새싹 잎이 자라나요! (총 %1개)";
    inline const QString STATUS_STAGE_2_FMT = "🌿 푸른 잎으로 무성해지는 중! (총 %1개)";
    inline const QString STATUS_STAGE_3_FMT = "🌳 지구를 살리는 울창한 나무 완성! (총 %1개)";
}

} // namespace UITheme

#endif // THEME_CONSTANTS_H