# 스마트 분리배출 키오스크 중앙 백엔드 서버 (Smart Recycling Central Backend)

스마트 분리배출 키오스크(Qt C++ GUI)와 사용자 모바일 앱(Android Kotlin) 간의 실시간 세션 연동 및 분리배출 집계, 리워드 포인트를 관리하는 고성능 비동기 중앙 백엔드 서버입니다.

---

## 📌 주요 기능 (Key Features)

- **실시간 디바이스 1:1 세션 동기화 (WebSocket)**
  - 키오스크(`kiosk`)와 모바일 앱(`mobile`) 간의 기기 룸 매핑.
  - QR 스캔 즉시 키오스크 화면을 배출 화면으로 실시간 전환 (`USER_AUTHENTICATED`).
  - 투입 완료 시 정산 결과 및 획득 포인트를 모바일 앱으로 실시간 푸시 (`RECYCLE_COMPLETE`).
- **Clean 3-Tier Layered Architecture**
  - **Presentation Layer (Routers)**: Thin Controller, DTO 유효성 검증 및 HTTP/WS 엔드포인트.
  - **Domain Service Layer (Services)**: 비즈니스 트랜잭션 오케스트레이션 및 웹소켓 알림 격리.
  - **Data Access Layer (Repositories)**: aiosqlite 비동기 SQL 캡슐화.
- **비동기 논블로킹 I/O 및 SQLite WAL 최적화**
  - `aiosqlite` 드라이버를 도입하여 비동기 이벤트 루프 멈춤(Stop-the-world) 원천 차단.
  - `PRAGMA journal_mode = WAL;`, `PRAGMA busy_timeout = 5000;` 적용으로 동시 읽기/쓰기 처리 성능 극대화 및 파일 락 충돌 방어.
  - Lifespan 이벤트를 통해 테이블 및 복합 인덱스(`idx_recycle_logs_user_id`, `idx_recycle_logs_created_at`, `idx_users_phone`) 자동 생성.
- **원자적 동시성 제어로 Race Condition 완전 방어**
  - **포인트 적립 Lost Update 방어**: `UPDATE users SET points = points + ? WHERE id = ? RETURNING points;` 단일 원자적 연산.
  - **포인트 차감 Double Spending / Overdraft 방어**: `UPDATE users SET points = points - ? WHERE id = ? AND points >= ? RETURNING points;` 조건부 원자적 연산.
  - **동시 가입 무결성 방어**: `INSERT ... ON CONFLICT(phone) DO UPDATE ... RETURNING ...` 원자적 UPSERT.
- **안정적인 WebSocket 세션 보호 (Connection Manager)**
  - `asyncio.Lock`을 통한 룸 딕셔너리 동시성 경합 차단.
  - 클라이언트 재접속 시 기존 세션을 명시적으로 종료(`close(1000)`)하여 파일 디스크립터 및 좀비 세션 누수 방지.
  - 소켓 인스턴스 일치 여부 대조 기반 해제 및 `try ... finally` 블록으로 안전한 자원 반환.
- **표준화된 예외 처리 및 클라이언트 규격 보존**
  - `AppException` 기반 도메인 커스텀 예외 체계.
  - 전역 예외 처리기로 500 에러 시 내부 스택트레이스 노출을 차단하면서 외부 클라이언트가 기대하는 `{"detail": "..."}` JSON 규격 100% 유지.

---

## 🏗️ 시스템 아키텍처 (System Architecture)

```
[Qt Kiosk Dashboard]                  [Android Mobile App]
        │ (WS / REST)                         │ (REST / WS)
        └───────────────────┬─────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 1. Presentation Layer (Routers & WebSocket Handlers)        │
│    - routers/auth.py, kiosk.py, users.py, websocket.py      │
│    - Pydantic v2 DTO 유효성 검증 & 전역 예외 처리기          │
└──────────────────────────────┬──────────────────────────────┘
                               │ Dependency Injection (Depends)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Domain Service Layer (Business Logic & Orchestration)    │
│    - services/auth_service.py (회원 가입/로그인 UPSERT)      │
│    - services/kiosk_service.py (QR 바인딩 & 배출 정산 조율) │
│    - services/user_service.py (프로필, 이력, 포인트 원자화) │
│    - connection_manager.py (asyncio.Lock, 좀비 세션 퇴출)   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Data Access Layer (Repositories & Async SQLite)          │
│    - repositories/user_repository.py, kiosk_repository.py   │
│    - repositories/log_repository.py                         │
│    - aiosqlite 비동기 논블로킹 엔진 (WAL Mode, Timeout 5s)  │
│    - 원자적 SQL 연산 (RETURNING 절, ON CONFLICT)            │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
                    [data/smart_recycle.db]
```

---

## 📂 디렉터리 구조 (Directory Structure)

```
server/
├── app/
│   ├── __init__.py
│   ├── config.py                 # SQLite 동시성 파라미터 및 서버 환경 설정
│   ├── connection_manager.py     # 스레드 안전 1:1 WebSocket 룸 관리자
│   ├── database.py               # aiosqlite 비동기 연결 풀 및 DDL/인덱스 초기화
│   ├── exceptions.py             # 비즈니스 커스텀 예외 및 표준 JSON 에러 핸들러
│   ├── schemas.py                # Pydantic v2 요청/응답 DTO
│   ├── repositories/             # Data Access 계층 (비동기 SQL 영속성)
│   │   ├── __init__.py
│   │   ├── kiosk_repository.py   # 키오스크 상태 조회/갱신
│   │   ├── log_repository.py     # 배출 로그 저장 및 인덱스 기반 페이징
│   │   └── user_repository.py    # 유저 조회, UPSERT, 원자적 포인트 연산
│   ├── services/                 # Business Service 계층 (도메인 로직)
│   │   ├── __init__.py
│   │   ├── auth_service.py       # 회원 인증 및 프로필 처리
│   │   ├── kiosk_service.py      # QR 바인딩, 배출 정산, 웹소켓 푸시 조율
│   │   └── user_service.py       # 유저 프로필, 배출 이력, 포인트 차감
│   └── routers/                  # Presentation 계층 (HTTP/WS 컨트롤러)
│       ├── __init__.py
│       ├── auth.py               # POST /api/auth/login
│       ├── kiosk.py              # POST /api/kiosk/bind, POST /api/recycle/submit
│       ├── users.py              # GET /api/users/{id}, GET /logs, POST /deduct
│       └── websocket.py          # WS /ws/kiosk/{bin_id}/{client_type}
├── data/
│   └── smart_recycle.db          # SQLite 데이터베이스 (WAL 파일 자동 생성)
├── tests/
│   └── mock_mobile_client.py     # 모바일 앱 E2E 시뮬레이터
├── main.py                       # FastAPI 앱 엔트리포인트 (Lifespan 관리)
├── requirements.txt              # 프로젝트 의존성 목록
└── README.md                     # 프로젝트 문서
```

---

## 🔌 API 및 WebSocket 명세 (Specification)

### 1. REST API

| Method | Endpoint                    | Description                       | Request Body                                             | Response Body                                                                     |
| :----- | :-------------------------- | :-------------------------------- | :------------------------------------------------------- | :-------------------------------------------------------------------------------- |
| `GET`  | `/health`                   | 서버 헬스체크                     | -                                                        | `{"status": "OK", "service": "..."}`                                              |
| `POST` | `/api/auth/login`           | 간편 로그인 및 신규 가입 (UPSERT) | `{"phone": str, "name": str}`                            | `UserResponse` (`id`, `phone`, `name`, `points`, `created_at`)                    |
| `POST` | `/api/kiosk/bind`           | 키오스크 QR 세션 바인딩           | `{"bin_id": int, "user_id": int}`                        | `{"status": "SUCCESS", "message": str, "bin_id": int, "user_id": int}`            |
| `POST` | `/api/recycle/submit`       | 분리배출 정산 및 로그 기록        | `RecycleSubmitRequest` (품목별 수량, 탄소절감량, 포인트) | `{"status": "SUCCESS", "log_id": int, "earned_points": int, "total_points": int}` |
| `GET`  | `/api/users/{user_id}`      | 사용자 단일 프로필 및 포인트 조회 | -                                                        | `UserResponse`                                                                    |
| `GET`  | `/api/users/{user_id}/logs` | 사용자 배출 상세 이력 목록 조회   | Query params (optional)                                  | `{"user_id": int, "total_count": int, "logs": [...]}`                             |
| `POST` | `/api/users/deduct`         | 리워드 포인트 안전 차감           | `{"user_id": int, "amount": int, "description": str}`    | `PointDeductResponse` (`deducted_amount`, `remaining_points`)                     |

### 2. WebSocket Specification

- **Endpoint**: `ws://<HOST>:<PORT>/ws/kiosk/{bin_id}/{client_type}`
  - `bin_id`: 키오스크 기기 식별자 (예: `1`)
  - `client_type`: `kiosk` (Qt 대시보드) 또는 `mobile` (Android 앱)
- **주요 푸시 이벤트**:
  - **`USER_AUTHENTICATED` (Server -> Kiosk)**: 모바일 앱에서 QR 바인딩 성공 시 Qt 화면 전환.
    ```json
    {
      "event": "USER_AUTHENTICATED",
      "user_id": 1,
      "name": "홍길동",
      "phone": "010-1234-5678",
      "points": 120
    }
    ```
  - **`RECYCLE_COMPLETE` (Server -> Mobile)**: 키오스크 배출 정산 완료 시 모바일 알림 푸시.
    ```json
    {
      "event": "RECYCLE_COMPLETE",
      "user_id": 1,
      "earned_points": 50,
      "total_points": 170,
      "carbon_saved_g": 120.5,
      "can_count": 2,
      "pet_count": 3,
      "paper_count": 1,
      "vinyl_count": 0
    }
    ```

---

## 🚀 설치 및 실행 방법 (Getting Started)

### 1. 가상환경 생성 및 활성화

```bash
# 가상환경 생성
python -m venv .venv

# 가상환경 활성화 (Windows PowerShell)
.venv\Scripts\Activate.ps1

# 가상환경 활성화 (Linux / macOS)
source .venv/bin/activate
```

### 2. 의존성 패키지 설치

```bash
pip install -r requirements.txt
```

### 3. 서버 실행

```bash
# 엔트리포인트 직접 실행 (기본 호스트: 0.0.0.0, 포트: 8000)
python main.py

# 또는 Uvicorn CLI로 실행
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

서버 실행 시 SQLite 테이블과 인덱스가 `data/smart_recycle.db`에 자동으로 생성(Lifespan 이벤트)됩니다.

- **Swagger API 문서 (Interactive Docs)**: `http://localhost:8000/docs`
- **ReDoc 문서**: `http://localhost:8000/redoc`

---

## 🧪 테스트 및 시뮬레이션 (Testing & Verification)

### 1. 모바일 클라이언트 E2E 시뮬레이터 실행

실제 모바일 앱의 로그인, QR 바인딩, WebSocket 대기실 입장, 정산 결과 수신 전 과정을 시뮬레이션합니다.

```bash
python tests/mock_mobile_client.py
```

### 2. 코드 품질 및 린트 검사

```bash
# ruff 정적 분석 실행
ruff check .
```
