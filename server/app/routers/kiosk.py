import sqlite3
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status

from app.connection_manager import manager
from app.database import get_db
from app.schemas import (
    KioskBindRequest,
    KioskBindResponse,
    RecycleSubmitRequest,
    RecycleSubmitResponse,
)

DatabaseDep = Annotated[sqlite3.Connection, Depends(get_db)]

router = APIRouter(tags=["Kiosk & Recycling"])


# ============================================================================
# 1. QR 스캔 세션 바인딩 및 키오스크 화면 전환 푸시
# ============================================================================
@router.post(
    "/api/kiosk/bind", response_model=KioskBindResponse, status_code=status.HTTP_200_OK
)
async def bind_kiosk_user(
    payload: KioskBindRequest,
    db: DatabaseDep,
) -> KioskBindResponse:
    """QR 스캔 후 모바일 앱과 키오스크를 1:1 바인딩하고 키오스크 화면을 전환."""
    cursor = db.cursor()

    # 1. 기기(키오스크) 존재 여부 검증
    cursor.execute("SELECT id, status FROM kiosks WHERE id = ?", (payload.bin_id,))
    kiosk = cursor.fetchone()
    if not kiosk:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {payload.bin_id}인 키오스크를 찾을 수 없습니다.",
        )

    # 2. 유저 존재 여부 및 최신 정보 조회
    cursor.execute(
        "SELECT id, phone, name, points FROM users WHERE id = ?", (payload.user_id,)
    )
    user = cursor.fetchone()
    if not user:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {payload.user_id}인 유저를 찾을 수 없습니다.",
        )

    # 3. 키오스크 기기 상태 업데이트
    cursor.execute(
        "UPDATE kiosks SET status = 'RUNNING', updated_at = datetime('now', 'localtime') WHERE id = ?",
        (payload.bin_id,),
    )
    db.commit()

    # 4. 키오스크(Qt)로 실시간 인증 성공 이벤트 푸시
    event_payload = {
        "event": "USER_AUTHENTICATED",
        "user_id": user["id"],
        "name": user["name"] if user["name"] else "회원",
        "phone": user["phone"],
        "points": user["points"],
    }
    await manager.send_to_kiosk(payload.bin_id, event_payload)

    return KioskBindResponse(
        status="SUCCESS",
        message="키오스크 세션이 성공적으로 활성화되었습니다.",
        bin_id=payload.bin_id,
        user_id=payload.user_id,
    )


# ============================================================================
# 2. 분리배출 투입 완료 및 결과 모바일 푸시
# ============================================================================
@router.post(
    "/api/recycle/submit",
    response_model=RecycleSubmitResponse,
    status_code=status.HTTP_200_OK,
)
async def submit_recycle_result(
    payload: RecycleSubmitRequest,
    db: DatabaseDep,
) -> RecycleSubmitResponse:
    """투입된 분리수거 품목 정산, 로그 기록, 포인트 지급 및 모바일 푸시."""
    cursor = db.cursor()

    # 1. 키오스크 존재 여부 확인
    cursor.execute("SELECT id FROM kiosks WHERE id = ?", (payload.bin_id,))
    if not cursor.fetchone():
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"ID가 {payload.bin_id}인 키오스크를 찾을 수 없습니다.",
        )

    total_user_points: int | None = None

    # 2. 회원인 경우 포인트 업데이트 및 검증
    if payload.user_id is not None:
        cursor.execute("SELECT id, points FROM users WHERE id = ?", (payload.user_id,))
        user = cursor.fetchone()
        if not user:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"ID가 {payload.user_id}인 유저를 찾을 수 없습니다.",
            )

        total_user_points = user["points"] + payload.earned_points
        cursor.execute(
            "UPDATE users SET points = ? WHERE id = ?",
            (total_user_points, payload.user_id),
        )

    # 3. 분리배출 로그 INSERT (4대 품목 및 탄소 절감량)
    cursor.execute(
        """
        INSERT INTO recycle_logs (
            user_id, bin_id, can_count, pet_count, paper_count, vinyl_count,
            carbon_saved_g, earned_points
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            payload.user_id,
            payload.bin_id,
            payload.can_count,
            payload.pet_count,
            payload.paper_count,
            payload.vinyl_count,
            payload.carbon_saved_g,
            payload.earned_points,
        ),
    )
    log_id = cursor.lastrowid

    # 4. 키오스크 기기 상태 IDLE 복구
    cursor.execute(
        "UPDATE kiosks SET status = 'IDLE', updated_at = datetime('now', 'localtime') WHERE id = ?",
        (payload.bin_id,),
    )
    db.commit()

    # 5. 모바일 앱(Android)으로 정산 완료 이벤트 푸시
    mobile_event = {
        "event": "RECYCLE_COMPLETE",
        "user_id": payload.user_id,
        "earned_points": payload.earned_points,
        "total_points": total_user_points,
        "carbon_saved_g": payload.carbon_saved_g,
        "can_count": payload.can_count,
        "pet_count": payload.pet_count,
        "paper_count": payload.paper_count,
        "vinyl_count": payload.vinyl_count,
    }
    await manager.send_to_mobile(payload.bin_id, mobile_event)

    return RecycleSubmitResponse(
        status="SUCCESS",
        log_id=log_id,
        earned_points=payload.earned_points,
        total_points=total_user_points,
    )
