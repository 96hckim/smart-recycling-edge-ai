# 📦 PC Kiosk Dashboard - Build & Environment Requirements

본 문서는 **PC Kiosk Dashboard**(`pc_dashboard`) 빌드 및 실행에 필요한 시스템 요구사항, Qt 모듈 의존성, 하드웨어 통신 규격을 명시합니다.

---

## 1. Toolchain & Language Specification

- **Language Standard**: C++17 (`CONFIG += c++17`)
- **Operating System**: Windows 10 / 11 (64-bit) (Linux 호환)
- **Supported Compilers**:
  - **MinGW-w64** (GCC 9.0 이상 / MSYS2 UCRT64 권장)
  - **MSVC** (Visual Studio 2019 / 2022, v142 / v143 도구 집합)
- **Build System**: `qmake` (Qt 5.15.x LTS 권장 / Qt 6.x 호환)

---

## 2. Qt Framework Dependencies

`pc_dashboard.pro`에 정의된 필수 Qt 모듈입니다.

```qmake
QT += core gui widgets network websockets
```

| 모듈명           | 주요 컴포넌트                                             | 용도                                        |
| :--------------- | :-------------------------------------------------------- | :------------------------------------------ |
| **`core`**       | `QObject`, `QTimer`, `QJsonDocument`, `QJsonObject`       | 이벤트 처리, 타이머, JSON 직렬화            |
| **`gui`**        | `QPixmap`, `QPainter`, `QMovie`, `QFont`, `QColor`        | 비전 프레임 디코딩, BBox 렌더링, 애니메이션 |
| **`widgets`**    | `QMainWindow`, `QStackedWidget`, `QLabel`, `QProgressBar` | 키오스크 3단계 UI 및 적재함 게이지          |
| **`network`**    | `QTcpSocket`, `QNetworkAccessManager`, `QNetworkReply`    | Jetson 비전 스트림 수신, 백엔드 REST 통신   |
| **`websockets`** | `QWebSocket`                                              | 중앙 서버 키오스크 실시간 QR 인증 세션 연동 |

---

## 3. Bundled & Third-Party Libraries

- **Nayuki QR Code Generator**:
  - 경로: `utils/qrcodegen.hpp`, `utils/qrcodegen.cpp`
  - 라이선스: MIT License
  - 비고: C++ 소스 코드로 프로젝트 내 번들링 완료 (외부 설치 불필요)
- **UI Font Resource**:
  - Pretendard (`UITheme::FONT_FAMILY`)
  - 시스템 미설치 시 기본 Sans-Serif 폰트로 자동 폴백

---

## 4. Hardware & Network Interfaces

- **Jetson Orin Nano (Vision AI Edge)**:
  - IP / Port: `10.10.15.48:9000` (TCP Socket)
  - 패킷 규격: 8-Byte Big-Endian Header `[img_size(4B)][json_size(4B)]` + JPEG + JSON
- **Central Backend (FastAPI)**:
  - Host / Port: `10.10.15.8:8000`
  - WebSocket: `ws://10.10.15.8:8000/ws/kiosk/{bin_id}/kiosk`
  - REST API: `POST http://10.10.15.8:8000/api/recycle/submit`
