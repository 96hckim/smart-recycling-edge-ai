#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <QRect>
#include <QString>
#include <QVector>

namespace Config {
// 1. 네트워크 통신 설정
constexpr char DEFAULT_JETSON_IP[] = "192.168.0.10";
constexpr quint16 JETSON_PORT = 9000;
constexpr int SOCKET_BUFFER_RESERVE = 512 * 1024;
constexpr int AUTO_RECONNECT_INTERVAL_MS = 2000;
constexpr quint32 HEADER_SIZE = 8; // ImgSize 4B + JsonSize 4B

// 2. AI 판정 & Debounce 설정
constexpr int STABLE_FRAME_THRESHOLD = 5; // 5프레임 연속 동일 시 확정
constexpr int RESULT_DISPLAY_TIMEOUT_SEC = 5; // 결과 화면 유지 시간

// 3. 4종 품목별 리워드 포인트 테이블
constexpr int POINT_CAN = 50;
constexpr int POINT_PET = 50;
constexpr int POINT_PAPER = 30;
constexpr int POINT_GENERAL = 0; // 일반쓰레기는 0P

// 4. 품목당 탄소 배출 절감량 (g CO2)
constexpr double CARBON_CAN_G = 25.0;
constexpr double CARBON_PET_G = 15.2;
constexpr double CARBON_PAPER_G = 8.5;
constexpr double CARBON_GENERAL_G = 0.0;
}

// 4종 품목 열거형
enum class RecycleCategory {
    UNKNOWN = -1,
    CAN = 0,
    PET = 1,
    PAPER = 2,
    GENERAL = 3
};

// 키오스크 FSM 상태
enum class KioskState {
    IDLE,
    AUTH_WAIT,
    RECYCLING,
    RESULT
};

// YOLO 검출 객체
struct Detection {
    int classId;
    QString className;
    double confidence;
    QRect box;
};

// Jetson 수신 프레임 메타데이터
struct FrameMetadata {
    double timestamp;
    double fps;
    double inferMs;
    QVector<Detection> detections;
};

// 세션 최종 정산 결과
struct SessionSummary {
    bool isMember;
    QString userName;
    int canCount { 0 };
    int petCount { 0 };
    int paperCount { 0 };
    int generalCount { 0 };
    int totalPoints { 0 };
    double totalCarbonG { 0.0 };
};

#endif // APP_CONFIG_H