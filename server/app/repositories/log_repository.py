"""분리배출 이력(recycle_logs) 생성 및 페이지네이션 조회 영속성 모듈."""

import aiosqlite


class LogRepository:
    """배출 정산 내역 영속화 및 사용자별 이력 조회 저장소."""

    def __init__(self, db: aiosqlite.Connection) -> None:
        self.db = db

    async def create_log(
        self,
        bin_id: int,
        user_id: int | None,
        can_count: int,
        pet_count: int,
        paper_count: int,
        vinyl_count: int,
        carbon_saved_g: float,
        earned_points: int,
    ) -> int:
        """분리배출 내역 영속화 및 자동 생성된 PK(log_id) 반환."""
        query = """
            INSERT INTO recycle_logs (
                user_id, bin_id, can_count, pet_count, paper_count, vinyl_count,
                carbon_saved_g, earned_points
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """
        async with self.db.execute(
            query,
            (
                user_id,
                bin_id,
                can_count,
                pet_count,
                paper_count,
                vinyl_count,
                carbon_saved_g,
                earned_points,
            ),
        ) as cursor:
            log_id = cursor.lastrowid
            await self.db.commit()
            return log_id

    async def get_logs_by_user_id(
        self,
        user_id: int,
        limit: int | None = None,
        offset: int = 0,
    ) -> tuple[list[aiosqlite.Row], int]:
        """특정 사용자의 배출 이력 목록(최신순 정렬) 및 총 누적 건수 페이징 조회."""
        # 1. 페이지네이션 메타데이터용 전체 레코드 수 집계
        async with self.db.execute(
            "SELECT COUNT(*) FROM recycle_logs WHERE user_id = ?",
            (user_id,),
        ) as cursor:
            count_row = await cursor.fetchone()
            total_count = count_row[0] if count_row else 0

        # 2. 복합 인덱스(idx_recycle_logs_created_at)를 활용한 최신순 정렬 및 슬라이싱
        query = """
            SELECT
                id, bin_id, can_count, pet_count, paper_count, vinyl_count,
                carbon_saved_g, earned_points, created_at

            FROM recycle_logs
            WHERE user_id = ?
            ORDER BY created_at DESC, id DESC
        """
        params: list = [user_id]
        if limit is not None:
            query += " LIMIT ? OFFSET ?"
            params.extend([limit, offset])

        async with self.db.execute(query, tuple(params)) as cursor:
            rows = await cursor.fetchall()
            return rows, total_count
