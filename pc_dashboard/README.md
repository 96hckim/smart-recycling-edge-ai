# 🖥️ Smart Recycling Edge-AI - PC Kiosk Dashboard

지능형 분리수거 키오스크를 위한 Qt/C++ 기반 실시간 AI 모니터링 및 사용자 대시보드 애플리케이션입니다.  
NVIDIA Jetson Orin Nano 엣지 디바이스로부터 고속 영상 스트림과 비전 메타데이터를 TCP로 수신하여 BBox/라벨을 오버레이하고, 중앙 관리 서버(FastAPI)와 WebSocket 및 REST API로 연동되어 사용자 인증 및 배출량 정산을 처리합니다.

---

## 📋 System Requirements & Prerequisites

C++/Qt 프로젝트는 Python의 `requirements.txt`와 달리 단일 패키지 매니저가 아닌 **컴파일러 규격, Qt 프레임워크 모듈, 시스템 네트워크 환경**을 기준으로 의존성을 구성합니다.

### 1. Development & Runtime Environment

| 항목                   | 최소 사양 / 권장 사양                                         | 설명                                                        |
| :--------------------- | :------------------------------------------------------------ | :---------------------------------------------------------- |
| **Operating System**   | Windows 10 / 11 (64-bit)                                      | Linux (Ubuntu 20.04/22.04) 크로스 빌드 호환                 |
| **C++ Standard**       | **C++17 이상**                                                | `std::clamp`, `if constexpr` 등 모던 C++ 문법 사용          |
| **Compiler Toolchain** | • MinGW-w64 (GCC 9.0+ / UCRT64)<br>• MSVC 2019 / 2022 (v142+) | `pc_dashboard.pro` 내 컴파일러별 Release 최적화 플래그 내장 |
| **Qt Framework**       | **Qt 5.15.x LTS** (권장) / Qt 6.x 호환                        | Qt Creator IDE 4.14+ 이상 권장                              |

---

### 2. Qt Dependencies (`pc_dashboard.pro`)

Qt 유지보수 툴(MaintenanceTool) 또는 패키지 매니저를 통해 아래 5개 모듈이 반드시 설치되어 있어야 합니다.

```qmake
QT += core gui widgets network websockets
```

| Qt 모듈          | 세부 기능 및 프로젝트 내 역할                                                                    |
| :--------------- | :----------------------------------------------------------------------------------------------- |
| **`core`**       | 이벤트 루프, 상태 머신 타이머(`QTimer`), JSON 메타데이터 직렬화/역직렬화(`QJsonDocument`)        |
| **`gui`**        | `QPixmap` 프레임 디코딩, `QPainter` 기반 BBox/배지 오버레이 드로잉, `QMovie` 에코 트리/폭죽 제어 |
| **`widgets`**    | `QMainWindow`, `QStackedWidget` 3단계 화면 전환(대기-배출-결과), 적재함 `QProgressBar` 게이지    |
| **`network`**    | `QTcpSocket` (Jetson Orin Nano와 비전 스트림 통신), `QNetworkAccessManager` (FastAPI REST 통신)  |
| **`websockets`** | `QWebSocket` (중앙 서버와의 실시간 키오스크 세션 및 모바일 QR 로그인 이벤트 동기화)              |

---

### 3. Third-Party Libraries & Bundled Assets

- **QR Code Generator**: [Nayuki QR Code Gen](https://www.nayuki.io/page/qr-code-generator-library) (C++ 버전)
  - 별도 외부 설치 없이 `utils/qrcodegen.hpp`, `utils/qrcodegen.cpp` 소스 코드로 내장 (MIT License)
- **UI Typography**: `Pretendard`
  - 시스템 미설치 시 기본 `QFont::SansSerif`로 폴백
- **Media Resources**:
  - `resources.qrc`에 `tree_grow.gif`, `confetti.gif` 등 시각화 리소스 바이너리 번들링 완료

---

### 4. Network & Hardware Prerequisites

대시보드가 정상적으로 AI 스트림 및 인증 세션을 수신하려면 아래 네트워크 엔드포인트 접근이 가능해야 합니다. (`configs/app_config.h`에서 수정 가능)

| 연동 시스템          | 프로토콜   | 기본 IP / Host | 기본 포트 | 엔드포인트 / 용도                                     |
| :------------------- | :--------- | :------------- | :-------- | :---------------------------------------------------- |
| **Jetson Orin Nano** | TCP Socket | `10.10.15.48`  | `9000`    | 영상 프레임 및 YOLO 추론 메타데이터 스트림 수신       |
| **FastAPI Backend**  | WebSocket  | `10.10.15.8`   | `8000`    | `ws://.../ws/kiosk/{bin_id}/kiosk` (모바일 QR 로그인) |
| **FastAPI Backend**  | HTTP REST  | `10.10.15.8`   | `8000`    | `POST /api/recycle/submit` (배출 결과 및 포인트 정산) |

---

## 🛠️ Build & Execution Guide

### Method 1. Qt Creator IDE (권장)

1. `Qt Creator` 실행
2. **File > Open File or Project...** 선택 후 `pc_dashboard/pc_dashboard.pro` 파일 열기
3. 키트(Kit) 선택 (예: `Desktop Qt 5.15.x MinGW 64-bit` 또는 `Desktop Qt 5.15.x MSVC2019 64-bit`)
4. 좌측 하단 빌드 모드를 **Release** 또는 **Debug**로 설정
5. `Ctrl + R` (Run) 또는 `Ctrl + B` (Build) 실행

### Method 2. Command Line Interface (CLI)

#### MinGW (MSYS2 / Windows)

```bash
# 1. 프로젝트 디렉토리 이동
cd pc_dashboard

# 2. Makefile 생성
qmake pc_dashboard.pro -spec win32-g++ "CONFIG+=release"

# 3. 컴파일 및 빌드
mingw32-make -j$(nproc)

# 4. 실행
./build/release/pc_dashboard.exe
```

#### MSVC (Visual Studio Developer Command Prompt)

```cmd
:: 1. 프로젝트 디렉토리 이동
cd pc_dashboard

:: 2. Makefile 생성
qmake pc_dashboard.pro -spec win32-msvc "CONFIG+=release"

:: 3. 컴파일 및 빌드
nmake

:: 4. 실행
.\build\release\pc_dashboard.exe
```

---

## 📂 Project Architecture

```plaintext
pc_dashboard/
├── configs/
│   ├── app_config.h            # 시스템 전역 상수, 임계치(0.6초/18프레임), 프로토콜 규격 및 데이터 구조체
│   └── theme_constants.h       # UI 테마 컬러 팔레트, BBox 스타일 및 다국어 텍스트 리소스
├── controllers/
│   ├── recycle_session_controller.h/.cpp  # 디바운싱 기반 비전 카운팅 및 세션 생명주기 제어
│   └── eco_tree_controller.h/.cpp         # 누적 배출량 연동 단계별 에코 트리 애니메이션 제어
├── network/
│   ├── jetson_client.h/.cpp    # Jetson TCP 8B 빅엔디안 헤더 언패킹 및 0-Copy 프레임 디코더
│   └── server_client.h/.cpp    # 중앙 서버 WebSocket 인증 리스너 및 REST API 정산 전송
├── ui/
│   ├── mainwindow.h/.cpp/.ui   # 상단 텔레메트리 바, 적재함 수위 캐시 및 화면 전환 중계
│   └── pages/
│       ├── idle_page.h/.cpp/.ui      # 대기 화면 (QPainter 기반 동적 딥링크 QR 렌더링)
│       ├── recycle_page.h/.cpp/.ui   # 배출 화면 (영상 비율 보정, BBox 및 안내 배너 표시)
│       └── result_page.h/.cpp/.ui    # 정산 화면 (숫자 롤링 애니메이션 및 자동 복귀 타이머)
├── utils/
│   └── qrcodegen.hpp/.cpp      # Nayuki QR Code Generation Engine (내장 C++ 라이브러리)
├── resources/
│   └── images/                 # GIF 애니메이션 및 이미지 리소스
├── main.cpp                    # 애플리케이션 진입점
├── pc_dashboard.pro            # Qt qmake 프로젝트 빌드 설정 파일
└── resources.qrc               # Qt 바이너리 리소스 정의 파일
```
