"""aiosqlite 기반 비동기 커넥션 풀링 및 DDL 인덱스 최적화 모듈."""

import logging
from collections.abc import AsyncGenerator

import aiosqlite

from app.config import settings

logger = logging.getLogger("uvicorn")


async def get_db_connection() -> aiosqlite.Connection:
    """WAL 모드 및 busy_timeout PRAGMA가 구성된 비동기 DB 커넥션 생성."""
    conn = await aiosqlite.connect(
        str(settings.DB_PATH),
        timeout=settings.DB_TIMEOUT_SECONDS,
    )
    conn.row_factory = aiosqlite.Row

    # 동시 읽기/쓰기 처리 성능 확보 및 동시성 락 충돌 방어
    await conn.execute("PRAGMA foreign_keys = ON;")
    await conn.execute("PRAGMA journal_mode = WAL;")
    await conn.execute("PRAGMA synchronous = NORMAL;")
    await conn.execute(f"PRAGMA busy_timeout = {settings.DB_BUSY_TIMEOUT_MS};")
    return conn


async def get_db() -> AsyncGenerator[aiosqlite.Connection, None]:
    """FastAPI 라우터 의존성 주입(Depends)용 비동기 세션 제너레이터."""
    conn = await get_db_connection()
    try:
        yield conn
    finally:
        await conn.close()


async def init_db() -> None:
    """서버 Lifespan 기동 시 스키마 생성 및 복합 인덱스 자동 구성."""
    settings.DATA_DIR.mkdir(parents=True, exist_ok=True)

    conn = await get_db_connection()
    try:
        # 1. 회원 정보 테이블
        await conn.execute(
            """
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                phone TEXT UNIQUE NOT NULL,
                name TEXT DEFAULT '회원',
                points INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT (datetime('now', 'localtime'))
            );
            """
        )

        # 2. 키오스크 상태 관리 테이블
        await conn.execute(
            """
            CREATE TABLE IF NOT EXISTS kiosks (
                id INTEGER PRIMARY KEY,
                status TEXT DEFAULT 'IDLE',
                updated_at TIMESTAMP DEFAULT (datetime('now', 'localtime'))
            );
            """
        )

        # 3. 분리배출 정산 로그 테이블
        await conn.execute(
            """
            CREATE TABLE IF NOT EXISTS recycle_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                bin_id INTEGER NOT NULL,
                can_count INTEGER DEFAULT 0,
                pet_count INTEGER DEFAULT 0,
                paper_count INTEGER DEFAULT 0,
                vinyl_count INTEGER DEFAULT 0,
                carbon_saved_g REAL DEFAULT 0.0,
                earned_points INTEGER DEFAULT 0,
                created_at TIMESTAMP DEFAULT (datetime('now', 'localtime')),
                FOREIGN KEY (user_id) REFERENCES users(id),
                FOREIGN KEY (bin_id) REFERENCES kiosks(id)
            );
            """
        )

        # 4. 배출 이력 및 사용자 조회 가속 인덱스
        await conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_recycle_logs_user_id ON recycle_logs(user_id);"
        )
        await conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_recycle_logs_created_at ON recycle_logs(created_at DESC, id DESC);"
        )
        await conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_users_phone ON users(phone);"
        )

        # 5. 기본 키오스크 레코드 보장
        await conn.execute(
            "INSERT OR IGNORE INTO kiosks (id, status) VALUES (?, 'IDLE');",
            (settings.DEFAULT_BIN_ID,),
        )
        await conn.commit()
        logger.info(
            "[DB Init] SQLite 테이블 및 성능 최적화 인덱스 초기화 완료 (WAL 모드)"
        )
    finally:
        await conn.close()
