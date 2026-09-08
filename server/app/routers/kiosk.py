"""키오스크 세션 바인딩 및 분리배출 정산 API 라우터 모듈."""

from typing import Annotated

import aiosqlite
from fastapi import APIRouter, Depends, status

from app.connection_manager import manager
from app.database import get_db
from app.repositories.kiosk_repository import KioskRepository
from app.repositories.log_repository import LogRepository
from app.repositories.user_repository import UserRepository
from app.schemas import (
    KioskBindRequest,
    KioskBindResponse,
    KioskCancelRequest,
    KioskCancelResponse,
    RecycleSubmitRequest,
    RecycleSubmitResponse,
)
from app.services.kiosk_service import KioskService

router = APIRouter(tags=["Kiosk & Recycling"])


def get_kiosk_service(
    db: Annotated[aiosqlite.Connection, Depends(get_db)],
) -> KioskService:
    """다중 리포지토리 및 웹소켓 싱글톤 매니저 주입 기반 KioskService 팩토리 의존성."""
    return KioskService(
        kiosk_repo=KioskRepository(db),
        user_repo=UserRepository(db),
        log_repo=LogRepository(db),
        ws_manager=manager,
    )


# ----------------------------------------------------------------------------
# 1. QR 스캔 세션 바인딩
# ----------------------------------------------------------------------------
@router.post(
    "/api/kiosk/bind", response_model=KioskBindResponse, status_code=status.HTTP_200_OK
)
async def bind_kiosk_user(
    payload: KioskBindRequest,
    service: Annotated[KioskService, Depends(get_kiosk_service)],
) -> KioskBindResponse:
    """모바일 앱의 키오스크 QR 스캔 검증 및 Qt 대시보드 화면 전환 트리거 엔드포인트."""
    return await service.bind_user(bin_id=payload.bin_id, user_id=payload.user_id)


# ----------------------------------------------------------------------------
# 2. 분리배출 투입 완료 정산
# ----------------------------------------------------------------------------
@router.post(
    "/api/recycle/submit",
    response_model=RecycleSubmitResponse,
    status_code=status.HTTP_200_OK,
)
async def submit_recycle_result(
    payload: RecycleSubmitRequest,
    service: Annotated[KioskService, Depends(get_kiosk_service)],
) -> RecycleSubmitResponse:
    """키오스크 투입 품목 정산, 포인트 원자적 가산, 이력 영속화 및 모바일 푸시 엔드포인트."""
    return await service.submit_recycle(payload)


# ----------------------------------------------------------------------------
# 3. 키오스크 세션 중도 취소 (양방향 연동)
# ----------------------------------------------------------------------------
@router.post(
    "/api/kiosk/cancel",
    response_model=KioskCancelResponse,
    status_code=status.HTTP_200_OK,
)
async def cancel_kiosk_session(
    payload: KioskCancelRequest,
    service: Annotated[KioskService, Depends(get_kiosk_service)],
) -> KioskCancelResponse:
    """키오스크 또는 모바일 앱에서 투입 취소 시 양측 디바이스를 동기화하고 대기 상태로 복구."""
    return await service.cancel_session(
        bin_id=payload.bin_id,
        user_id=payload.user_id,
        reason=payload.reason,
    )
