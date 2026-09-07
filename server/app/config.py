"""전역 서버 환경 설정 및 SQLite 동시성 파라미터 관리 모듈."""

from pathlib import Path


class Settings:
    """서버 네트워크, 파일 시스템 경로 및 DB 타임아웃 설정 컨테이너."""

    # 서버 루트 기준 절대 경로 산출
    BASE_DIR: Path = Path(__file__).resolve().parent.parent
    DATA_DIR: Path = BASE_DIR / "data"
    DB_PATH: Path = DATA_DIR / "smart_recycle.db"

    # API 서버 호스트 및 바인딩 포트
    HOST: str = "0.0.0.0"
    PORT: int = 8000

    # 기본 키오스크 식별자
    DEFAULT_BIN_ID: int = 1

    # SQLite 동시 쓰기 경합 완화용 대기 시간
    DB_TIMEOUT_SECONDS: float = 10.0
    DB_BUSY_TIMEOUT_MS: int = 5000  # database is locked 방어용 5초 대기


# 전역 설정 싱글톤 인스턴스
settings = Settings()
