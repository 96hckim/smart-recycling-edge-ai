#pragma once
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

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

// 순서: 종이(0) -> 캔(1) -> 페트(2) -> 비닐(3)
enum class RecycleCategory {
    UNKNOWN = -1,
    PAPER = 0,
    CAN = 1,
    PET = 2,
    VINYL = 3
};

// ============================================================================
// 2. 비즈니스 로직 및 시스템 설정
// ============================================================================
namespace Config {

// 품목 총 개수 및 Mock 모델 플래그
constexpr int CATEGORY_COUNT = 4;
constexpr bool USE_MOCK_RPS_MODEL = true;

// 네트워크 및 IPC 설정 (Jetson Orin Nano 통신)
constexpr char DEFAULT_JETSON_IP[] = "10.10.15.48";
constexpr quint16 JETSON_PORT = 9000;
constexpr int SOCKET_BUFFER_RESERVE = 512 * 1024;
constexpr int AUTO_RECONNECT_INTERVAL_MS = 2000;
constexpr quint32 HEADER_SIZE = 8;

// FastAPI 중앙 백엔드 통신 설정
constexpr char DEFAULT_BACKEND_HOST[] = "10.10.15.8";
constexpr quint16 DEFAULT_BACKEND_PORT = 8000;
constexpr int DEFAULT_BIN_ID = 1;

// 프로토콜 명령 키
constexpr char CMD_OPEN_BIN[] = "OPEN_BIN";
constexpr char KEY_CMD[] = "cmd";
constexpr char KEY_TARGET[] = "target";

// AI 판정 임계값
constexpr int STABLE_FRAME_THRESHOLD = 18; // 약 0.6초 유지 시 확정
constexpr double MIN_CONFIDENCE_THRESHOLD = 0.65; // 모호한 바운딩 박스 필터링

// 키오스크 물리 적재함 및 세션 타이머
constexpr int MAX_BIN_CAPACITY = 100;
constexpr int BIN_FULL_WARNING_PERCENT = 80;
constexpr int RESULT_DISPLAY_TIMEOUT_SEC = 5;
constexpr int RECYCLE_SESSION_TIMEOUT_SEC = 60; // 60초 미동작 시 리셋

namespace Demo {
    inline const QString MEMBER_USER_ID = "회원";
}

namespace Backend {
    inline const QString WS_URL_FMT = "ws://%1:%2/ws/kiosk/%3/kiosk";
    inline const QString API_SUBMIT_PATH = "http://%1:%2/api/recycle/submit";

    namespace Event {
        inline const QString USER_AUTHENTICATED = "USER_AUTHENTICATED";
        inline const QString EMERGENCY_STOP = "EMERGENCY_STOP";
    }

    namespace Key {
        inline const QString EVENT = "event";
        inline const QString USER_ID = "user_id";
        inline const QString NAME = "name";
        inline const QString PHONE = "phone";
        inline const QString POINTS = "points";
        inline const QString BIN_ID = "bin_id";
        inline const QString PAPER_COUNT = "paper_count";
        inline const QString CAN_COUNT = "can_count";
        inline const QString PET_COUNT = "pet_count";
        inline const QString VINYL_COUNT = "vinyl_count";
        inline const QString CARBON_SAVED = "carbon_saved_g";
        inline const QString EARNED_PTS = "earned_points";
        inline const QString LOG_ID = "log_id";
        inline const QString TOTAL_POINTS = "total_points";
    }
}

// 모바일 앱 딥링크(App Link) 규격
namespace Auth {
    inline const QString DEEPLINK_SCHEME = "smartrecycle://kiosk/auth";
    inline const QString DEEPLINK_PAYLOAD_FMT = "%1?bin_id=%2";
}

namespace EcoTree {
    inline constexpr int THRESHOLD_STAGE_1 = 2;
    inline constexpr int THRESHOLD_STAGE_2 = 4;

    inline constexpr double FRAME_RATIO_BASE = 0.30;
    inline constexpr double FRAME_RATIO_STAGE_1 = 0.55;
    inline constexpr double FRAME_RATIO_STAGE_2 = 0.80;
}

// ============================================================================
// 3. 재활용 품목 메타데이터 테이블 및 헬퍼 함수
// 순서: 종이 -> 캔 -> 페트 -> 비닐
// ============================================================================
struct ItemMeta {
    RecycleCategory category;
    const char* nameKo;
    const char* nameEn;
    int unitPoint;
    double unitCarbonG;
};

inline constexpr ItemMeta ITEM_METAS[CATEGORY_COUNT] = {
    { RecycleCategory::PAPER, "종이", "PAPER", 30, 8.5 },
    { RecycleCategory::CAN, "캔", "CAN", 50, 25.0 },
    { RecycleCategory::PET, "페트", "PET", 50, 15.2 },
    { RecycleCategory::VINYL, "비닐", "VINYL", 10, 5.0 }
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

inline RecycleCategory parseCategory(const QString& name)
{
    const QString upper = name.toUpper().trimmed();

    if constexpr (USE_MOCK_RPS_MODEL) {
        if (upper.contains("PAPER") || upper.contains("보"))
            return RecycleCategory::PAPER;
        if (upper.contains("ROCK") || upper.contains("주먹") || upper.contains("바위"))
            return RecycleCategory::CAN;
        if (upper.contains("SCISSOR") || upper.contains("가위"))
            return RecycleCategory::PET;
    }

    if (upper.contains("PAPER") || upper.contains("종이") || upper.contains("BOX"))
        return RecycleCategory::PAPER;
    if (upper.contains("CAN") || upper.contains("캔"))
        return RecycleCategory::CAN;
    if (upper.contains("PET") || upper.contains("PLASTIC") || upper.contains("페트"))
        return RecycleCategory::PET;
    if (upper.contains("VINYL") || upper.contains("비닐") || upper.contains("PLASTIC_BAG") || upper.contains("WRAP"))
        return RecycleCategory::VINYL;

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
    int paperCount { 0 };
    int canCount { 0 };
    int petCount { 0 };
    int vinylCount { 0 };
    int totalPoints { 0 };
    double totalCarbonG { 0.0 };

    void addItem(RecycleCategory cat, int count = 1)
    {
        switch (cat) {
        case RecycleCategory::PAPER:
            paperCount += count;
            break;
        case RecycleCategory::CAN:
            canCount += count;
            break;
        case RecycleCategory::PET:
            petCount += count;
            break;
        case RecycleCategory::VINYL:
            vinylCount += count;
            break;
        default:
            break;
        }
        recalculate();
    }

    void recalculate()
    {
        totalPoints = (paperCount * Config::getPoint(RecycleCategory::PAPER))
            + (canCount * Config::getPoint(RecycleCategory::CAN))
            + (petCount * Config::getPoint(RecycleCategory::PET))
            + (vinylCount * Config::getPoint(RecycleCategory::VINYL));

        totalCarbonG = (paperCount * Config::getCarbonG(RecycleCategory::PAPER))
            + (canCount * Config::getCarbonG(RecycleCategory::CAN))
            + (petCount * Config::getCarbonG(RecycleCategory::PET))
            + (vinylCount * Config::getCarbonG(RecycleCategory::VINYL));
    }

    void reset()
    {
        *this = SessionSummary();
    }
};

#endif // APP_CONFIG_H