"""키오스크 QR 세션 바인딩 및 분리배출 정산 비즈니스 로직 모듈."""

import logging

from app.connection_manager import ConnectionManager
from app.exceptions import KioskNotFoundError, UserNotFoundError
from app.repositories.kiosk_repository import KioskRepository
from app.repositories.log_repository import LogRepository
from app.repositories.user_repository import UserRepository
from app.schemas import (
    KioskBindResponse,
    RecycleSubmitRequest,
    RecycleSubmitResponse,
)

logger = logging.getLogger("uvicorn")


class KioskService:
    """기기 상태 전이, 포인트 원자적 적립 및 실시간 WebSocket 이벤트 중계 서비스."""

    def __init__(
        self,
        kiosk_repo: KioskRepository,
        user_repo: UserRepository,
        log_repo: LogRepository,
        ws_manager: ConnectionManager,
    ) -> None:
        self.kiosk_repo = kiosk_repo
        self.user_repo = user_repo
        self.log_repo = log_repo
        self.ws_manager = ws_manager

    async def bind_user(self, bin_id: int, user_id: int) -> KioskBindResponse:
        """모바일 앱 QR 스캔을 검증하여 키오스크와 바인딩하고 Qt 대시보드로 인증 이벤트 푸시."""
        kiosk = await self.kiosk_repo.get_by_id(bin_id)
        if not kiosk:
            raise KioskNotFoundError(bin_id)

        user = await self.user_repo.get_by_id(user_id)
        if not user:
            raise UserNotFoundError(user_id)

        await self.kiosk_repo.update_status(bin_id, "RUNNING")

        # Qt 클라이언트로 인증 성공 이벤트 푸시 (소켓 통신 미연결 상태가 HTTP 응답을 차단하지 않도록 비차단 처리)
        event_payload = {
            "event": "USER_AUTHENTICATED",
            "user_id": user["id"],
            "name": user["name"] if user["name"] else "회원",
            "phone": user["phone"],
            "points": user["points"],
        }
        sent = await self.ws_manager.send_to_kiosk(bin_id, event_payload)
        if not sent:
            logger.info(
                f"[KioskService] 키오스크 웹소켓 미연결 또는 전송 실패 (Room {bin_id})"
            )

        return KioskBindResponse(
            status="SUCCESS",
            message="키오스크 세션이 성공적으로 활성화되었습니다.",
            bin_id=bin_id,
            user_id=user_id,
        )

    async def submit_recycle(
        self, payload: RecycleSubmitRequest
    ) -> RecycleSubmitResponse:
        """투입 완료 품목 정산, 회원 포인트 원자적 적립, 이력 영속화 및 모바일 완료 푸시 일괄 처리."""
        kiosk = await self.kiosk_repo.get_by_id(payload.bin_id)
        if not kiosk:
            raise KioskNotFoundError(payload.bin_id)

        total_user_points: int | None = None

        # 회원 투입 시 DB 레벨 원자적 포인트 적립 (Lost Update 완전 방지)
        if payload.user_id is not None:
            user = await self.user_repo.get_by_id(payload.user_id)
            if not user:
                raise UserNotFoundError(payload.user_id)

            total_user_points = await self.user_repo.add_points_atomic(
                payload.user_id, payload.earned_points
            )

        # 배출 이력 상세 로그 영속화
        log_id = await self.log_repo.create_log(
            bin_id=payload.bin_id,
            user_id=payload.user_id,
            can_count=payload.can_count,
            pet_count=payload.pet_count,
            paper_count=payload.paper_count,
            vinyl_count=payload.vinyl_count,
            carbon_saved_g=payload.carbon_saved_g,
            earned_points=payload.earned_points,
        )

        # 키오스크 물리 상태 IDLE 복구
        await self.kiosk_repo.update_status(payload.bin_id, "IDLE")

        # 모바일 앱으로 세션 결과 비동기 이벤트 푸시 (전송 에러 격리)
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
        sent = await self.ws_manager.send_to_mobile(payload.bin_id, mobile_event)
        if not sent:
            logger.info(
                f"[KioskService] 모바일 웹소켓 미연결 또는 전송 실패 (Room {payload.bin_id})"
            )

        return RecycleSubmitResponse(
            status="SUCCESS",
            log_id=log_id,
            earned_points=payload.earned_points,
            total_points=total_user_points,
        )
