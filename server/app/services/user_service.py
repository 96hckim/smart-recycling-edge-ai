"""사용자 프로필 조회, 배출 이력 페이징 및 포인트 차감 비즈니스 로직 모듈."""

from app.exceptions import InsufficientPointsError, UserNotFoundError
from app.repositories.log_repository import LogRepository
from app.repositories.user_repository import UserRepository
from app.schemas import (
    PointDeductRequest,
    PointDeductResponse,
    RecycleLogItem,
    RecycleLogListResponse,
    UserResponse,
)


class UserService:
    """사용자 계정 상태 관리, 배출 히스토리 조합 및 포인트 안전 차감 서비스."""

    def __init__(
        self,
        user_repo: UserRepository,
        log_repo: LogRepository,
    ) -> None:
        self.user_repo = user_repo
        self.log_repo = log_repo

    async def get_profile(self, user_id: int) -> UserResponse:
        """단일 유저 프로필 및 최신 보유 포인트 조회."""
        user = await self.user_repo.get_by_id(user_id)
        if not user:
            raise UserNotFoundError(user_id)

        return UserResponse(
            id=user["id"],
            phone=user["phone"],
            name=user["name"] if user["name"] else "회원",
            points=user["points"],
            created_at=str(user["created_at"]),
        )

    async def get_recycle_logs(
        self,
        user_id: int,
        limit: int | None = None,
        offset: int = 0,
    ) -> RecycleLogListResponse:
        """복합 인덱스 기반 사용자 배출 로그 목록 및 총 레코드 수 페이징 조회."""
        user = await self.user_repo.get_by_id(user_id)
        if not user:
            raise UserNotFoundError(user_id)

        rows, total_count = await self.log_repo.get_logs_by_user_id(
            user_id, limit=limit, offset=offset
        )

        logs = [
            RecycleLogItem(
                id=row["id"],
                bin_id=row["bin_id"],
                can_count=row["can_count"],
                pet_count=row["pet_count"],
                paper_count=row["paper_count"],
                vinyl_count=row["vinyl_count"],
                carbon_saved_g=row["carbon_saved_g"],
                earned_points=row["earned_points"],
                created_at=str(row["created_at"]),
            )
            for row in rows
        ]

        return RecycleLogListResponse(
            user_id=user_id,
            total_count=total_count,
            logs=logs,
        )

    async def deduct_points(self, payload: PointDeductRequest) -> PointDeductResponse:
        """DB 레벨 원자적 감산을 통한 초과 출금(Overdraft) 방어 및 포인트 차감 처리."""
        user = await self.user_repo.get_by_id(payload.user_id)
        if not user:
            raise UserNotFoundError(payload.user_id)

        # 잔액 조건(points >= amount) 검증을 포함한 원자적 쿼리 실행
        remaining_points = await self.user_repo.deduct_points_atomic(
            payload.user_id, payload.amount
        )

        # 잔액 부족으로 UPDATE 행 수가 0인 경우 예외 발생
        if remaining_points is None:
            raise InsufficientPointsError(
                current_points=user["points"],
                requested_points=payload.amount,
            )

        return PointDeductResponse(
            status="SUCCESS",
            user_id=payload.user_id,
            deducted_amount=payload.amount,
            remaining_points=remaining_points,
            description=payload.description or "포인트 사용",
        )
