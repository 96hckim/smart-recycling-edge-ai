"""사용자 계정 및 DB 레벨 원자적(Atomic) 포인트 트랜잭션 관리 모듈."""

import aiosqlite


class UserRepository:
    """회원 CRUD 및 Race Condition 방어 원자적 포인트 연산 저장소."""

    def __init__(self, db: aiosqlite.Connection) -> None:
        self.db = db

    async def get_by_id(self, user_id: int) -> aiosqlite.Row | None:
        """사용자 고유 PK(user_id) 기반 단일 회원 조회."""
        async with self.db.execute(
            "SELECT id, phone, name, points, created_at FROM users WHERE id = ?",
            (user_id,),
        ) as cursor:
            return await cursor.fetchone()

    async def get_by_phone(self, phone: str) -> aiosqlite.Row | None:
        """유니크 인덱스(idx_users_phone) 기반 휴대폰 번호 단일 회원 조회."""
        async with self.db.execute(
            "SELECT id, phone, name, points, created_at FROM users WHERE phone = ?",
            (phone,),
        ) as cursor:
            return await cursor.fetchone()

    async def upsert_user(self, phone: str, name: str) -> aiosqlite.Row:
        """전화번호 기준 신규 회원 생성 또는 이름 갱신 (UPSERT 원자적 처리)."""
        # RETURNING 절을 활용해 추가 SELECT 쿼리 없이 갱신된 레코드 단일 왕복 반환
        query = """
            INSERT INTO users (phone, name, points)
            VALUES (?, ?, 0)
            ON CONFLICT(phone) DO UPDATE SET
                name = CASE
                    WHEN excluded.name != '회원' THEN excluded.name
                    ELSE users.name
                END
            RETURNING id, phone, name, points, created_at;
        """

        async with self.db.execute(query, (phone, name)) as cursor:
            row = await cursor.fetchone()
            await self.db.commit()
            return row

    async def add_points_atomic(self, user_id: int, points_to_add: int) -> int | None:
        """동시성 분실 갱신(Lost Update)을 원천 차단하는 DB 레벨 원자적 포인트 가산."""
        query = """
            UPDATE users
            SET points = points + ?
            WHERE id = ?
            RETURNING points;
        """
        async with self.db.execute(query, (points_to_add, user_id)) as cursor:
            row = await cursor.fetchone()
            if row is not None:
                await self.db.commit()
                return row["points"]
            return None

    async def deduct_points_atomic(self, user_id: int, amount: int) -> int | None:
        """동시 인출 및 초과 출금(Overdraft)을 방어하는 조건부 원자적 포인트 감산."""
        # 잔액 조건(points >= amount) 충족 시에만 갱신 (조건 불일치 시 0행 갱신 후 None 반환)
        query = """
            UPDATE users
            SET points = points - ?
            WHERE id = ? AND points >= ?
            RETURNING points;
        """
        async with self.db.execute(query, (amount, user_id, amount)) as cursor:
            row = await cursor.fetchone()
            if row is not None:
                await self.db.commit()
                return row["points"]
            return None
