import sqlite3
from collections.abc import Generator

from app.config import settings


def get_db_connection() -> sqlite3.Connection:
    """단일 SQLite 커넥션 생성 (외래키 활성화 및 Row 팩토리 적용)."""
    conn = sqlite3.connect(
        str(settings.DB_PATH),
        check_same_thread=False,  # FastAPI 비동기/멀티스레드 환경 대응
        timeout=10.0,  # SQLite 파일 락 대기 시간 설정 (초)
    )
    # 컬럼명으로 데이터 접근 가능하도록 설정 (예: row["points"])
    conn.row_factory = sqlite3.Row
    # SQLite 외래키 제약조건 강제 활성화
    conn.execute("PRAGMA foreign_keys = ON;")
    return conn


def get_db() -> Generator[sqlite3.Connection, None, None]:
    """FastAPI 라우터 의존성 주입(Depends)용 제너레이터."""
    conn = get_db_connection()
    try:
        yield conn
    finally:
        conn.close()
