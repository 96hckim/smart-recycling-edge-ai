#pragma once
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRect>
#include <QString>
#include <QVector>

// ============================================================================
// 1. 시스템 전역 상태 열거형
// ============================================================================
enum class KioskState
{
    IDLE,
    AUTH_WAIT,
    RECYCLING,
    RESULT,
    MAINTENANCE,
    ERROR_STATE
};

// 순서: 종이(0) -> 캔(1) -> 페트(2) -> 비닐(3)
enum class RecycleCategory
{
    UNKNOWN = -1,
    PAPER = 0,
    CAN = 1,
    PET = 2,
    VINYL = 3
};

// ============================================================================
// 2. 비즈니스 로직 및 시스템 설정
// ============================================================================
namespace Config
{

    constexpr int CATEGORY_COUNT = 4;
    constexpr bool USE_MOCK_RPS_MODEL = true; // [수정] 실제 재활용 객체 검출 모델 기준으로 기본값 변경

    // 네트워크 및 IPC 설정 (Jetson Orin Nano 통신)
    constexpr char DEFAULT_JETSON_IP[] = "10.10.15.48";
    constexpr quint16 JETSON_PORT = 9000;
    constexpr int SOCKET_BUFFER_RESERVE = 512 * 1024;
    constexpr int AUTO_RECONNECT_INTERVAL_MS = 2000;
    constexpr quint32 HEADER_SIZE = 8;
    constexpr quint32 MAX_IMAGE_SIZE = 10 * 1024 * 1024;  // 10MB 상한 (OOM 방지)
    constexpr quint32 MAX_JSON_SIZE = 64 * 1024;          // 64KB 상한
    constexpr int MAX_BUFFER_CAPACITY = 20 * 1024 * 1024; // 20MB 최대 버퍼 용량

    // FastAPI 중앙 백엔드 통신 설정
    constexpr char DEFAULT_BACKEND_HOST[] = "10.10.15.8";
    constexpr quint16 DEFAULT_BACKEND_PORT = 8000;
    constexpr int DEFAULT_BIN_ID = 1;

    // AI 판정 임계값
    constexpr int STABLE_FRAME_THRESHOLD = 18; // 약 0.6초 유지 시 확정
    constexpr double MIN_CONFIDENCE_THRESHOLD = 0.65;

    // 키오스크 물리 적재함 및 세션 타이머
    constexpr int MAX_BIN_CAPACITY = 100;
    constexpr int BIN_FULL_WARNING_PERCENT = 80;
    constexpr int RESULT_DISPLAY_TIMEOUT_SEC = 10;
    constexpr int RECYCLE_SESSION_TIMEOUT_SEC = 60;

    namespace Demo
    {
        inline const QString MEMBER_USER_ID = "회원";
    }

    namespace Backend
    {
        inline const QString WS_URL_FMT = "ws://%1:%2/ws/kiosk/%3/kiosk";
        inline const QString API_SUBMIT_PATH = "http://%1:%2/api/recycle/submit";

        namespace Event
        {
            inline const QString USER_AUTHENTICATED = "USER_AUTHENTICATED";
            inline const QString EMERGENCY_STOP = "EMERGENCY_STOP";
        }

        namespace Key
        {
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

    namespace Auth
    {
        inline const QString DEEPLINK_SCHEME = "smartrecycle://kiosk/auth";
        inline const QString DEEPLINK_PAYLOAD_FMT = "%1?bin_id=%2";
    }

    namespace EcoTree
    {
        inline constexpr int THRESHOLD_STAGE_1 = 2;
        inline constexpr int THRESHOLD_STAGE_2 = 4;
        inline constexpr double FRAME_RATIO_BASE = 0.30;
        inline constexpr double FRAME_RATIO_STAGE_1 = 0.55;
        inline constexpr double FRAME_RATIO_STAGE_2 = 0.80;
    }

    // ============================================================================
    // 3. 재활용 품목 메타데이터 테이블 및 헬퍼 함수
    // ============================================================================
    struct ItemMeta
    {
        RecycleCategory category;
        const char *nameKo;
        const char *nameEn;
        int unitPoint;
        double unitCarbonG;
    };

    inline constexpr ItemMeta ITEM_METAS[CATEGORY_COUNT] = {
        {RecycleCategory::PAPER, "종이", "PAPER", 30, 8.5},
        {RecycleCategory::CAN, "캔", "CAN", 50, 25.0},
        {RecycleCategory::PET, "페트", "PET", 50, 15.2},
        {RecycleCategory::VINYL, "비닐", "VINYL", 10, 5.0}};

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

    inline const char *getCategoryNameEn(RecycleCategory cat)
    {
        int idx = static_cast<int>(cat);
        return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].nameEn : "UNKNOWN";
    }

    inline const char *getCategoryNameKo(RecycleCategory cat)
    {
        int idx = static_cast<int>(cat);
        return (idx >= 0 && idx < CATEGORY_COUNT) ? ITEM_METAS[idx].nameKo : "미확인";
    }

    inline RecycleCategory parseCategory(const QString &name)
    {
        const QString upper = name.toUpper().trimmed();

        if constexpr (USE_MOCK_RPS_MODEL)
        {
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
// Jetson <-> Qt TCP 프로토콜 메타데이터 키 상수
// ============================================================================
namespace Config::JetsonProtocol
{
    inline constexpr char KEY_TIMESTAMP[] = "timestamp";
    inline constexpr char KEY_FPS[] = "fps";
    inline constexpr char KEY_INFER_MS[] = "infer_ms";
    inline constexpr char KEY_DETECTIONS[] = "detections";
    inline constexpr char KEY_BIN_LEVELS[] = "bin_levels";
    inline constexpr char KEY_DOOR[] = "door";

    // 도어 키 및 상태 상수
    inline constexpr char KEY_ITEM[] = "item";
    inline constexpr char KEY_STATE[] = "state";
    inline constexpr char STATE_OPEN[] = "OPEN";
    inline constexpr char STATE_CLOSED[] = "CLOSED";

    // 적재함 키
    inline constexpr char KEY_PAPER[] = "paper";
    inline constexpr char KEY_CAN[] = "can";
    inline constexpr char KEY_PET[] = "pet";
    inline constexpr char KEY_VINYL[] = "vinyl";

    // [추가] Detection 필드 키 상수
    inline constexpr char KEY_CLASS_ID[] = "class_id";
    inline constexpr char KEY_CLASS_NAME[] = "class_name";
    inline constexpr char KEY_CONFIDENCE[] = "confidence";
    inline constexpr char KEY_BOX[] = "box";
}

// ============================================================================
// 4. 프레임 메타데이터 및 하드웨어 텔레메트리 구조체
// ============================================================================

struct BinStatus
{
    int paper{0};
    int can{0};
    int pet{0};
    int vinyl{0};

    bool operator==(const BinStatus &o) const
    {
        return paper == o.paper && can == o.can && pet == o.pet && vinyl == o.vinyl;
    }

    bool operator!=(const BinStatus &o) const
    {
        return !(*this == o);
    }

    static BinStatus fromJson(const QJsonObject &obj)
    {
        using namespace Config::JetsonProtocol;
        BinStatus status;
        status.paper = obj.value(KEY_PAPER).toInt();
        status.can = obj.value(KEY_CAN).toInt();
        status.pet = obj.value(KEY_PET).toInt();
        status.vinyl = obj.value(KEY_VINYL).toInt();
        return status;
    }
};

struct HardwareDoorStatus
{
    QString item{"ALL"};
    bool isOpen{false};

    static HardwareDoorStatus fromJson(const QJsonObject &obj)
    {
        using namespace Config::JetsonProtocol;
        HardwareDoorStatus status;
        status.item = obj.value(KEY_ITEM).toString("ALL");
        status.isOpen = (obj.value(KEY_STATE).toString().toUpper() == STATE_OPEN);
        return status;
    }
};

struct Detection
{
    int classId{-1};
    QString className{};
    double confidence{0.0};
    QRect box{};
    RecycleCategory category{RecycleCategory::UNKNOWN};

    static Detection fromJson(const QJsonObject &obj)
    {
        using namespace Config::JetsonProtocol;
        Detection d;
        d.classId = obj.value(KEY_CLASS_ID).toInt();
        d.className = obj.value(KEY_CLASS_NAME).toString();
        d.confidence = obj.value(KEY_CONFIDENCE).toDouble();
        d.category = Config::parseCategory(d.className);

        const QJsonArray bArr = obj.value(KEY_BOX).toArray();
        if (bArr.size() >= 4)
        {
            d.box = QRect(QPoint(bArr[0].toInt(), bArr[1].toInt()),
                          QPoint(bArr[2].toInt(), bArr[3].toInt()));
        }
        return d;
    }
};

struct FrameMetadata
{
    double timestamp{0.0};
    double fps{0.0};
    double inferMs{0.0};
    QVector<Detection> detections{};
    BinStatus binLevels{};
    HardwareDoorStatus door{};

    static FrameMetadata fromJson(const QJsonObject &obj)
    {
        using namespace Config::JetsonProtocol;
        FrameMetadata meta;
        meta.timestamp = obj.value(KEY_TIMESTAMP).toDouble();
        meta.fps = obj.value(KEY_FPS).toDouble();
        meta.inferMs = obj.value(KEY_INFER_MS).toDouble();

        const QJsonArray detArray = obj.value(KEY_DETECTIONS).toArray();
        for (const QJsonValue &val : detArray)
        {
            if (val.isObject())
            {
                meta.detections.append(Detection::fromJson(val.toObject()));
            }
        }

        if (obj.contains(KEY_BIN_LEVELS) && obj.value(KEY_BIN_LEVELS).isObject())
        {
            meta.binLevels = BinStatus::fromJson(obj.value(KEY_BIN_LEVELS).toObject());
        }

        if (obj.contains(KEY_DOOR) && obj.value(KEY_DOOR).isObject())
        {
            meta.door = HardwareDoorStatus::fromJson(obj.value(KEY_DOOR).toObject());
        }

        return meta;
    }
};

struct SessionSummary
{
    bool isMember{false};
    QString userName{};
    int paperCount{0};
    int canCount{0};
    int petCount{0};
    int vinylCount{0};
    int totalPoints{0};
    double totalCarbonG{0.0};

    void addItem(RecycleCategory cat, int count = 1)
    {
        switch (cat)
        {
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
        totalPoints = (paperCount * Config::getPoint(RecycleCategory::PAPER)) + (canCount * Config::getPoint(RecycleCategory::CAN)) + (petCount * Config::getPoint(RecycleCategory::PET)) + (vinylCount * Config::getPoint(RecycleCategory::VINYL));

        totalCarbonG = (paperCount * Config::getCarbonG(RecycleCategory::PAPER)) + (canCount * Config::getCarbonG(RecycleCategory::CAN)) + (petCount * Config::getCarbonG(RecycleCategory::PET)) + (vinylCount * Config::getCarbonG(RecycleCategory::VINYL));
    }

    void reset()
    {
        *this = SessionSummary();
    }
};

#endif // APP_CONFIG_H