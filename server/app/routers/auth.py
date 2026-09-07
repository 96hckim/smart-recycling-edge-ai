"""모바일 회원 인증 및 간편 로그인 API 라우터 모듈."""

from typing import Annotated

import aiosqlite
from fastapi import APIRouter, Depends, status

from app.database import get_db
from app.repositories.user_repository import UserRepository
from app.schemas import LoginRequest, UserResponse
from app.services.auth_service import AuthService

router = APIRouter(prefix="/api/auth", tags=["Auth"])


def get_auth_service(
    db: Annotated[aiosqlite.Connection, Depends(get_db)],
) -> AuthService:
    """DB 커넥션 주입 기반 AuthService 팩토리 의존성."""
    return AuthService(UserRepository(db))


@router.post("/login", response_model=UserResponse, status_code=status.HTTP_200_OK)
async def login_or_register(
    payload: LoginRequest,
    service: Annotated[AuthService, Depends(get_auth_service)],
) -> UserResponse:
    """휴대폰 번호 기반 회원 조회 또는 신규 가입(UPSERT) 처리 엔드포인트."""
    return await service.login_or_register(
        phone=payload.phone,
        name=payload.name,
    )
