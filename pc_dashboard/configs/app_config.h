#pragma once
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <QRect>
#include <QString>
#include <QVector>
#include <array>

// ============================================================================
// 1. 키오스크 FSM 상태 정의
// ============================================================================
enum class KioskState {
    IDLE, // 대기 화면 (QR 스캔 또는 비회원 터치 대기)
    AUTH_WAIT, // 사용자 인증/로그인 대기
    RECYCLING, // 재활용품 투입 및 실시간 AI 비전 인식 중
    RESULT, // 세션 정산 결과 표시
    MAINTENANCE, // 수거함 만석 또는 관리자 점검 모드
    ERROR_STATE // 비전 통신 단절 등 시스템 장애 상태
};

// ============================================================================
// 2. 재활용 품목 분류 및 메타데이터
// ============================================================================
enum class RecycleCategory {
    UNKNOWN = -1,
    CAN = 0,
    PET = 1,
    PAPER = 2,
    GENERAL = 3
};

namespace Config {
// 4종 품목 총 개수 (배열 크기 정의용)
constexpr int CATEGORY_COUNT = 4;

// 네트워크 통신 설정
constexpr char DEFAULT_JETSON_IP[] = "192.168.0.10";
constexpr quint16 JETSON_PORT = 9000;
constexpr int SOCKET_BUFFER_RESERVE = 512 * 1024;
constexpr int AUTO_RECONNECT_INTERVAL_MS = 2000;
constexpr quint32 HEADER_SIZE = 8; // ImgSize 4B + JsonSize 4B

// AI 판정 & Debounce 설정
constexpr int STABLE_FRAME_THRESHOLD = 5; // 5프레임 연속 동일 시 확정
constexpr int RESULT_DISPLAY_TIMEOUT_SEC = 5; // 결과 화면 유지 시간(초)
constexpr double MIN_CONFIDENCE_THRESHOLD = 0.50; // 유효 감지 최소 신뢰도

// 수거함 적재 용량 기준 (단위: 개수 또는 센서 임계치)
constexpr int MAX_BIN_CAPACITY = 100;
constexpr int BIN_FULL_WARNING_PERCENT = 80;

// 품목별 메타 정보 구조체
struct ItemMeta {
    RecycleCategory category;
    const char* nameKo;
    const char* nameEn;
    int unitPoint;
    double unitCarbonG;
    const char* themeColor;
};

// 품목 메타데이터 테이블 (1:1 매핑)
inline constexpr ItemMeta ITEM_METAS[CATEGORY_COUNT] = {
    { RecycleCategory::CAN, "캔", "CAN", 50, 25.0, "#10B981" },
    { RecycleCategory::PET, "페트", "PET", 50, 15.2, "#38BDF8" },
    { RecycleCategory::PAPER, "종이", "PAPER", 30, 8.5, "#F59E0B" },
    { RecycleCategory::GENERAL, "일반", "GENERAL", 0, 0.0, "#94A3B8" }
};

// --- 편의 유틸리티 인라인 함수 ---
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

// YOLO 인식 문자열 -> Enum 변환기
inline RecycleCategory parseCategory(const QString& name)
{
    const QString upper = name.toUpper().trimmed();
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
}

// ============================================================================
// 3. 비전 추론 및 세션 데이터 구조체
// ============================================================================

// YOLO 검출 객체
struct Detection {
    int classId { -1 };
    QString className { };
    double confidence { 0.0 };
    QRect box { };
    RecycleCategory category { RecycleCategory::UNKNOWN };
};

// Jetson 수신 프레임 메타데이터
struct FrameMetadata {
    double timestamp { 0.0 };
    double fps { 0.0 };
    double inferMs { 0.0 };
    QVector<Detection> detections { };
};

// 세션 최종 정산 결과 모델
struct SessionSummary {
    bool isMember { false };
    QString userName { };
    int canCount { 0 };
    int petCount { 0 };
    int paperCount { 0 };
    int generalCount { 0 };
    int totalPoints { 0 };
    double totalCarbonG { 0.0 };

    // 품목 투입 시 수량 및 리워드 자동 누적
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

    // 리워드 및 탄소 절감량 재계산
    void recalculate()
    {
        totalPoints = isMember ? (canCount * Config::getPoint(RecycleCategory::CAN) + petCount * Config::getPoint(RecycleCategory::PET) + paperCount * Config::getPoint(RecycleCategory::PAPER)) : 0;

        totalCarbonG = (canCount * Config::getCarbonG(RecycleCategory::CAN)) + (petCount * Config::getCarbonG(RecycleCategory::PET)) + (paperCount * Config::getCarbonG(RecycleCategory::PAPER));
    }

    void reset()
    {
        *this = SessionSummary();
    }
};

#endif // APP_CONFIG_H