import sqlite3
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status

from app.database import get_db
from app.schemas import (
    PointDeductRequest,
    PointDeductResponse,
    RecycleLogItem,
    RecycleLogListResponse,
    UserResponse,
)

DatabaseDep = Annotated[sqlite3.Connection, Depends(get_db)]

router = APIRouter(prefix="/api/users", tags=["Users"])


@router.get("/{user_id}", response_model=UserResponse, status_code=status.HTTP_200_OK)
def get_user_profile(
    user_id: int,
    db: DatabaseDep,
) -> UserResponse:
    """모바일 홈 화면용 단일 유저 프로필 및 현재 포인트 조회."""
    cursor = db.cursor()
    cursor.execute(
        "SELECT id, phone, name, points, created_at FROM users WHERE id = ?",
        (user_id,),
    )
    user = cursor.fetchone()

    if not user:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {user_id}인 유저를 찾을 수 없습니다.",
        )

    return UserResponse(
        id=user["id"],
        phone=user["phone"],
        name=user["name"] if user["name"] else "회원",
        points=user["points"],
        created_at=user["created_at"],
    )


@router.get(
    "/{user_id}/logs",
    response_model=RecycleLogListResponse,
    status_code=status.HTTP_200_OK,
)
def get_user_recycle_logs(
    user_id: int,
    db: DatabaseDep,
) -> RecycleLogListResponse:
    """모바일 앱 이력 탭용 분리배출 기록 목록 조회 (최신순 정렬)."""
    cursor = db.cursor()

    # 유저 존재 여부 먼저 확인
    cursor.execute("SELECT id FROM users WHERE id = ?", (user_id,))
    if not cursor.fetchone():
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {user_id}인 유저를 찾을 수 없습니다.",
        )

    # 4대 품목 로그 최신순 조회
    cursor.execute(
        """
        SELECT 
            id, bin_id, can_count, pet_count, paper_count, vinyl_count,
            carbon_saved_g, earned_points, created_at
        FROM recycle_logs
        WHERE user_id = ?
        ORDER BY created_at DESC, id DESC
        """,
        (user_id,),
    )
    rows = cursor.fetchall()

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
        total_count=len(logs),
        logs=logs,
    )


@router.post(
    "/deduct",
    response_model=PointDeductResponse,
    status_code=status.HTTP_200_OK,
    summary="포인트 차감",
)
def deduct_user_points(
    payload: PointDeductRequest,
    db: DatabaseDep,
) -> PointDeductResponse:
    """앱에서 상품 구매 시 유저 포인트를 검증하고 차감."""
    cursor = db.cursor()

    # 1. 유저 존재 및 현재 포인트 조회
    cursor.execute(
        "SELECT id, points FROM users WHERE id = ?",
        (payload.user_id,),
    )
    user = cursor.fetchone()

    if not user:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {payload.user_id}인 유저를 찾을 수 없습니다.",
        )

    current_points = user["points"]

    # 2. 포인트 부족 검증
    if current_points < payload.amount:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"포인트가 부족합니다. (현재: {current_points}P, 요청: {payload.amount}P)",
        )

    # 3. 포인트 차감 및 DB 커밋
    remaining_points = current_points - payload.amount
    cursor.execute(
        "UPDATE users SET points = ? WHERE id = ?",
        (remaining_points, payload.user_id),
    )
    db.commit()

    return PointDeductResponse(
        status="SUCCESS",
        user_id=payload.user_id,
        deducted_amount=payload.amount,
        remaining_points=remaining_points,
        description=payload.description or "포인트 사용",
    )
