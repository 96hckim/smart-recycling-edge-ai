#pragma once
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <QColor>
#include <QRect>
#include <QString>
#include <QVector>

// ============================================================================
// 1. 시스템 전역 상태 열거형
// ============================================================================
enum class KioskState {
    IDLE,
    AUTH_WAIT,
    RECYCLING,
    RESULT,
    MAINTENANCE,
    ERROR_STATE
};

enum class RecycleCategory {
    UNKNOWN = -1,
    CAN = 0,
    PET = 1,
    PAPER = 2,
    GENERAL = 3
};

// ============================================================================
// 2. 비즈니스 로직 및 설정 네임스페이스
// ============================================================================
namespace Config {

// 품목 총 개수 및 모델 스위칭 플래그
constexpr int CATEGORY_COUNT = 4;
constexpr bool USE_MOCK_RPS_MODEL = true;

// 네트워크 및 IPC 설정 (Jetson Orin Nano 통신)
constexpr char DEFAULT_JETSON_IP[] = "10.10.15.48";
constexpr quint16 JETSON_PORT = 9000;
constexpr int SOCKET_BUFFER_RESERVE = 512 * 1024;
constexpr int AUTO_RECONNECT_INTERVAL_MS = 2000;
constexpr quint32 HEADER_SIZE = 8;

// 프로토콜 명령 키
constexpr char CMD_OPEN_BIN[] = "OPEN_BIN";
constexpr char KEY_CMD[] = "cmd";
constexpr char KEY_TARGET[] = "target";

// AI 판정 임계값
constexpr int STABLE_FRAME_THRESHOLD = 18; // 약 0.6초 유지 시 확정
constexpr double MIN_CONFIDENCE_THRESHOLD = 0.65; // 모호한 바운딩 박스 필터링

// 키오스크 물리 적재함 용량
constexpr int MAX_BIN_CAPACITY = 100;
constexpr int BIN_FULL_WARNING_PERCENT = 80;
constexpr int RESULT_DISPLAY_TIMEOUT_SEC = 5;

// 서브 네임스페이스별 설정
namespace Demo {
    inline const QString MEMBER_USER_ID = "MEMBER_DEMO_USER";
}

namespace Connection {
    inline const QString STATUS_ONLINE = "● AI VISION ONLINE";
    inline const QString STATUS_OFFLINE = "○ AI VISION OFFLINE";
}

namespace Telemetry {
    inline const QString FORMAT_STR = "FPS: %1 | Infer: %2ms | Network Latency: %3ms | Jetson Stream Port: %4";
}

namespace VisionRender {
    inline constexpr int BADGE_FONT_SIZE = 22; // 텍스트 크기 확대
    inline constexpr int BOX_PEN_WIDTH = 4; // 바운딩 박스 선 두께 강화
    inline constexpr int BADGE_PAD_X = 14; // 배지 좌우 여백 확장
    inline constexpr int BADGE_PAD_Y = 8; // 배지 상하 여백 확장
}

namespace Result {
    inline constexpr int COUNTDOWN_INTERVAL_MS = 1000;
    inline constexpr int ANIM_POINTS_DURATION_MS = 900;
    inline constexpr int ANIM_CARBON_DURATION_MS = 1100;

    // 폭죽 애니메이션 설정
    inline const QString CONFETTI_RESOURCE_PATH = ":/images/confetti.gif";
    inline constexpr int CONFETTI_SPEED = 100; // 100% 정속
}

namespace EcoTree {
    inline const QString RESOURCE_PATH = ":/images/tree_grow.gif";
    inline constexpr int MOVIE_SPEED = 200;
    inline constexpr int DEFAULT_FRAME_COUNT = 120;

    // 개수 임계값 (0개: 잎 없는 기본 나무, 1~2개: 새싹 잎, 3~4개: 풍성한 잎, 5개 이상: 완성)
    inline constexpr int THRESHOLD_STAGE_1 = 2;
    inline constexpr int THRESHOLD_STAGE_2 = 4;

    // 프레임 진행 비율
    inline constexpr double FRAME_RATIO_BASE = 0.30; // 0개 대기 상태: 잎 없는 기본 나무 프레임
    inline constexpr double FRAME_RATIO_STAGE_1 = 0.55; // 1단계: 잎이 조금 돋아남
    inline constexpr double FRAME_RATIO_STAGE_2 = 0.80; // 2단계: 잎이 무성해짐
}

// ============================================================================
// 3. 재활용 품목 메타데이터 테이블 및 헬퍼 함수
// ============================================================================
struct ItemMeta {
    RecycleCategory category;
    const char* nameKo;
    const char* nameEn;
    int unitPoint;
    double unitCarbonG;
    const char* themeColor;
};

inline constexpr ItemMeta ITEM_METAS[CATEGORY_COUNT] = {
    { RecycleCategory::CAN, "캔", "CAN", 50, 25.0, "#10B981" },
    { RecycleCategory::PET, "페트", "PET", 50, 15.2, "#38BDF8" },
    { RecycleCategory::PAPER, "종이", "PAPER", 30, 8.5, "#F59E0B" },
    { RecycleCategory::GENERAL, "일반", "GENERAL", 0, 0.0, "#94A3B8" }
};

inline int getPoint(RecycleCategory cat)
{
    int idx = static_cast<int>(cat);
    return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].unitPoint : 0;
}

inline double getCarbonG(RecycleCategory cat)
{
    int idx = static_cast<int>(cat);
    return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].unitCarbonG : 0.0;
}

inline const char* getCategoryNameEn(RecycleCategory cat)
{
    int idx = static_cast<int>(cat);
    return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].nameEn : "UNKNOWN";
}

inline const char* getCategoryNameKo(RecycleCategory cat)
{
    int idx = static_cast<int>(cat);
    return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].nameKo : "미확인";
}

inline QColor getCategoryColor(RecycleCategory cat)
{
    int idx = static_cast<int>(cat);
    return (idx >= 0 && idx < CATEGORY_COUNT) ? QColor(ITEM_METAS[idx].themeColor) : QColor("#10B981");
}

inline RecycleCategory parseCategory(const QString& name)
{
    const QString upper = name.toUpper().trimmed();

    if constexpr (USE_MOCK_RPS_MODEL) {
        if (upper.contains("ROCK") || upper.contains("주먹") || upper.contains("바위"))
            return RecycleCategory::CAN;
        if (upper.contains("SCISSOR") || upper.contains("가위"))
            return RecycleCategory::PET;
        if (upper.contains("PAPER") || upper.contains("보"))
            return RecycleCategory::PAPER;
    }

    if (upper.contains("CAN") || upper.contains("캔"))
        return RecycleCategory::CAN;
    if (upper.contains("PET") || upper.contains("PLASTIC") || upper.contains("페트"))
        return RecycleCategory::PET;
    if (upper.contains("PAPER") || upper.contains("종이") || upper.contains("BOX"))
        return RecycleCategory::PAPER;
    if (upper.contains("GENERAL") || upper.contains("TRASH") || upper.contains("일반"))
        return RecycleCategory::GENERAL;

    return RecycleCategory::UNKNOWN;
}

} // namespace Config

// ============================================================================
// 4. 프레임 메타데이터 및 세션 집계 구조체
// ============================================================================
struct Detection {
    int classId { -1 };
    QString className { };
    double confidence { 0.0 };
    QRect box { };
    RecycleCategory category { RecycleCategory::UNKNOWN };
};

struct FrameMetadata {
    double timestamp { 0.0 };
    double fps { 0.0 };
    double inferMs { 0.0 };
    QVector<Detection> detections { };
};

struct SessionSummary {
    bool isMember { false };
    QString userName { };
    int canCount { 0 };
    int petCount { 0 };
    int paperCount { 0 };
    int generalCount { 0 };
    int totalPoints { 0 };
    double totalCarbonG { 0.0 };

    void addItem(RecycleCategory cat, int count = 1)
    {
        switch (cat) {
        case RecycleCategory::CAN:
            canCount += count;
            break;
        case RecycleCategory::PET:
            petCount += count;
            break;
        case RecycleCategory::PAPER:
            paperCount += count;
            break;
        case RecycleCategory::GENERAL:
            generalCount += count;
            break;
        default:
            break;
        }
        recalculate();
    }

    void recalculate()
    {
        totalPoints = (canCount * Config::getPoint(RecycleCategory::CAN))
            + (petCount * Config::getPoint(RecycleCategory::PET))
            + (paperCount * Config::getPoint(RecycleCategory::PAPER));

        totalCarbonG = (canCount * Config::getCarbonG(RecycleCategory::CAN))
            + (petCount * Config::getCarbonG(RecycleCategory::PET))
            + (paperCount * Config::getCarbonG(RecycleCategory::PAPER));
    }

    void reset()
    {
        *this = SessionSummary();
    }
};

#endif // APP_CONFIG_H