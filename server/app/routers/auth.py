import sqlite3
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status

from app.database import get_db
from app.schemas import LoginRequest, UserResponse

DatabaseDep = Annotated[sqlite3.Connection, Depends(get_db)]

router = APIRouter(prefix="/api/auth", tags=["Auth"])


@router.post("/login", response_model=UserResponse, status_code=status.HTTP_200_OK)
def login_or_register(
    payload: LoginRequest,
    db: DatabaseDep,
) -> UserResponse:
    phone = payload.phone.strip()
    name = payload.name.strip() if payload.name else "회원"

    if not phone:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="휴대폰 번호는 필수 입력 항목입니다.",
        )

    cursor = db.cursor()

    # 1. 기존 유저 조회 (name 포함)
    cursor.execute(
        "SELECT id, phone, name, points, created_at FROM users WHERE phone = ?",
        (phone,),
    )
    user = cursor.fetchone()

    # 2. 신규 등록
    if not user:
        cursor.execute(
            "INSERT INTO users (phone, name, points) VALUES (?, ?, 0)",
            (phone, name),
        )
        db.commit()

        cursor.execute(
            "SELECT id, phone, name, points, created_at FROM users WHERE id = ?",
            (cursor.lastrowid,),
        )
        user = cursor.fetchone()
    else:
        # 기존 유저의 이름이 업데이트된 경우
        if name != "회원" and user["name"] != name:
            cursor.execute(
                "UPDATE users SET name = ? WHERE id = ?",
                (name, user["id"]),
            )
            db.commit()
            cursor.execute(
                "SELECT id, phone, name, points, created_at FROM users WHERE id = ?",
                (user["id"],),
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
        name=user["name"],
        points=user["points"],
        created_at=str(user["created_at"]),
    )
