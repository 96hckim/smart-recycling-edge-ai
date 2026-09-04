from pathlib import Path


class Settings:
    # 1. 프로젝트 및 DB 경로 설정
    # BASE_DIR: server/ 폴더 기준 절대 경로 계산
    BASE_DIR: Path = Path(__file__).resolve().parent.parent
    DATA_DIR: Path = BASE_DIR / "data"
    DB_PATH: Path = DATA_DIR / "smart_recycle.db"

    # 2. 서버 실행 호스트 및 포트
    HOST: str = "0.0.0.0"
    PORT: int = 8000

    # 3. 기본 키오스크 식별자
    DEFAULT_BIN_ID: int = 1


# 싱글톤 인스턴스로 어디서든 import settings로 사용
settings = Settings()
