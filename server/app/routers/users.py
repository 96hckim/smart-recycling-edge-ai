import sqlite3
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status

from app.database import get_db
from app.schemas import RecycleLogItem, RecycleLogListResponse, UserResponse

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
        "SELECT id, phone, points, created_at FROM users WHERE id = ?",
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
