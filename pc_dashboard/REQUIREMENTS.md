# 📦 PC Kiosk Dashboard - Build & Environment Requirements (MSYS2 UCRT64)

본 문서는 **PC Kiosk Dashboard**(`pc_dashboard`) 빌드 및 실행에 필요한 MSYS2 UCRT64 패키지 요구사항, Qt 모듈 의존성, 하드웨어 통신 규격을 명시합니다.

---

## 1. Primary Toolchain & Environment

- **Platform**: Windows 10 / 11 (64-bit)
- **Environment**: **MSYS2 UCRT64** (`ucrt64.exe`)
- **Language Standard**: C++17 (`CONFIG += c++17`)
- **Compiler**: MinGW-w64 UCRT64 GCC 13+
- **Build System**: `qmake` (Qt 5.15.x LTS for UCRT64)

---

## 2. One-Liner Dependency Installation (MSYS2 pacman)

Python의 `pip install -r requirements.txt`처럼, MSYS2 UCRT64 쉘에서 아래 명령어를 실행하여 빌드 환경을 즉시 구축할 수 있습니다:

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-make \
    mingw-w64-ucrt-x86_64-qt5-base \
    mingw-w64-ucrt-x86_64-qt5-websockets
```

> **(선택 사항) Qt Creator IDE**:
>
> ```bash
> pacman -S --needed mingw-w64-ucrt-x86_64-qt-creator
> ```

---

## 3. Qt Framework Module Mapping (`pc_dashboard.pro`)

```qmake
QT += core gui widgets network websockets
```

| Qt 모듈          | MSYS2 UCRT64 패키지명                  | 주요 컴포넌트 및 역할                                 |
| :--------------- | :------------------------------------- | :---------------------------------------------------- |
| **`core`**       | `mingw-w64-ucrt-x86_64-qt5-base`       | `QObject`, `QTimer`, `QJsonDocument`, 이벤트 루프     |
| **`gui`**        | `mingw-w64-ucrt-x86_64-qt5-base`       | `QPixmap`, `QPainter`, `QMovie` (BBox/애니메이션)     |
| **`widgets`**    | `mingw-w64-ucrt-x86_64-qt5-base`       | `QMainWindow`, `QStackedWidget`, `QProgressBar`       |
| **`network`**    | `mingw-w64-ucrt-x86_64-qt5-base`       | `QTcpSocket` (Jetson 스트림), `QNetworkAccessManager` |
| **`websockets`** | `mingw-w64-ucrt-x86_64-qt5-websockets` | `QWebSocket` (중앙 서버 실시간 QR 인증 세션)          |

---

## 4. Bundled & Third-Party Libraries

- **Nayuki QR Code Generator**:
  - 경로: `utils/qrcodegen.hpp`, `utils/qrcodegen.cpp`
  - 라이선스: MIT License
  - 비고: C++ 소스 코드로 프로젝트 내 번들링 완료 (외부 설치 불필요)
- **UI Font Resource**:
  - Pretendard (`UITheme::FONT_FAMILY`)
  - 시스템 미설치 시 기본 Sans-Serif 폰트로 자동 폴백

---

## 5. Hardware & Network Interfaces

- **Jetson Orin Nano (Vision AI Edge)**:
  - IP / Port: `10.10.15.48:9000` (TCP Socket)
  - 패킷 규격: 8-Byte Big-Endian Header `[img_size(4B)][json_size(4B)]` + JPEG + JSON
- **Central Backend (FastAPI)**:
  - Host / Port: `10.10.15.8:8000`
  - WebSocket: `ws://10.10.15.8:8000/ws/kiosk/{bin_id}/kiosk`
  - REST API: `POST http://10.10.15.8:8000/api/recycle/submit`

---

## 6. Build Commands (MSYS2 UCRT64 Shell)

```bash
cd /d/Project/smart-recycling-edge-ai/pc_dashboard
qmake pc_dashboard.pro "CONFIG+=release"
make -j$(nproc)
./build/release/pc_dashboard.exe
```
