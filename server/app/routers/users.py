"""사용자 프로필, 배출 이력 조회 및 리워드 포인트 차감 API 라우터 모듈."""

from typing import Annotated

import aiosqlite
from fastapi import APIRouter, Depends, status

from app.database import get_db
from app.repositories.log_repository import LogRepository
from app.repositories.user_repository import UserRepository
from app.schemas import (
    PointDeductRequest,
    PointDeductResponse,
    RecycleLogListResponse,
    UserResponse,
)
from app.services.user_service import UserService

router = APIRouter(prefix="/api/users", tags=["Users"])


def get_user_service(
    db: Annotated[aiosqlite.Connection, Depends(get_db)],
) -> UserService:
    """DB 커넥션 주입 기반 UserService 팩토리 의존성."""
    return UserService(
        user_repo=UserRepository(db),
        log_repo=LogRepository(db),
    )


@router.get("/{user_id}", response_model=UserResponse, status_code=status.HTTP_200_OK)
async def get_user_profile(
    user_id: int,
    service: Annotated[UserService, Depends(get_user_service)],
) -> UserResponse:
    """단일 회원 프로필 및 보유 잔여 포인트 조회 엔드포인트."""
    return await service.get_profile(user_id)


@router.get(
    "/{user_id}/logs",
    response_model=RecycleLogListResponse,
    status_code=status.HTTP_200_OK,
)
async def get_user_recycle_logs(
    user_id: int,
    service: Annotated[UserService, Depends(get_user_service)],
) -> RecycleLogListResponse:
    """인덱스 기반 사용자별 분리배출 정산 로그 목록(최신순) 페이징 조회 엔드포인트."""
    return await service.get_recycle_logs(user_id)


@router.post(
    "/deduct",
    response_model=PointDeductResponse,
    status_code=status.HTTP_200_OK,
    summary="포인트 차감",
)
async def deduct_user_points(
    payload: PointDeductRequest,
    service: Annotated[UserService, Depends(get_user_service)],
) -> PointDeductResponse:
    """모바일 앱 상품 교환 시 동중 출금(Double Spending) 방어 원자적 포인트 차감 엔드포인트."""
    return await service.deduct_points(payload)
