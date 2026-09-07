"""키오스크 기기 상태 조회 및 갱신 영속성 계층 모듈."""

import aiosqlite


class KioskRepository:
    """키오스크 엔티티(kiosks 테이블) SQLite CRUD 래퍼."""

    def __init__(self, db: aiosqlite.Connection) -> None:
        self.db = db

    async def get_by_id(self, bin_id: int) -> aiosqlite.Row | None:
        """키오스크 ID 기준 단일 상태 조회."""
        async with self.db.execute(
            "SELECT id, status, updated_at FROM kiosks WHERE id = ?",
            (bin_id,),
        ) as cursor:
            return await cursor.fetchone()

    async def update_status(self, bin_id: int, status: str) -> None:
        """키오스크 동작 상태(IDLE, RUNNING 등) 및 로컬 갱신 시각 동기화."""
        await self.db.execute(
            "UPDATE kiosks SET status = ?, updated_at = datetime('now', 'localtime') WHERE id = ?",
            (status, bin_id),
        )
        await self.db.commit()
