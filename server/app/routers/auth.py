import sqlite3
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status

from app.database import get_db
from app.schemas import LoginRequest, UserResponse

# B008 규칙 준수를 위한 타입 별칭 정의
DatabaseDep = Annotated[sqlite3.Connection, Depends(get_db)]

router = APIRouter(prefix="/api/auth", tags=["Auth"])


@router.post("/login", response_model=UserResponse, status_code=status.HTTP_200_OK)
def login_or_register(
    payload: LoginRequest,
    db: DatabaseDep,
) -> UserResponse:
    """휴대폰 번호 단일 식별자 기반 간이 로그인 및 자동 회원가입."""
    phone = payload.phone.strip()
    if not phone:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="휴대폰 번호는 필수 입력 항목입니다.",
        )

    cursor = db.cursor()

    # 1. 기존 유저 조회
    cursor.execute(
        "SELECT id, phone, points, created_at FROM users WHERE phone = ?",
        (phone,),
    )
    user = cursor.fetchone()

    # 2. 존재하지 않는 경우 신규 등록
    if not user:
        cursor.execute(
            "INSERT INTO users (phone, points) VALUES (?, 0)",
            (phone,),
        )
        db.commit()

        # 방금 생성된 유저 재조회
        cursor.execute(
            "SELECT id, phone, points, created_at FROM users WHERE id = ?",
            (cursor.lastrowid,),
        )
        user = cursor.fetchone()

    if not user:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="유저 데이터 생성 또는 조회에 실패했습니다.",
        )

    return UserResponse(
        id=user["id"],
        phone=user["phone"],
        points=user["points"],
        created_at=user["created_at"],
    )
