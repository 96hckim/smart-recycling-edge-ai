"""도메인 비즈니스 커스텀 예외 및 통일된 JSON 에러 핸들러 모듈."""

import logging

from fastapi import FastAPI, HTTPException, Request, status
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

logger = logging.getLogger("uvicorn")


class AppException(Exception):
    """애플리케이션 공통 비즈니스 예외 기반 클래스."""

    def __init__(
        self,
        status_code: int = status.HTTP_400_BAD_REQUEST,
        detail: str = "요청 처리 중 오류가 발생했습니다.",
        error_code: str = "BAD_REQUEST",
    ) -> None:
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail
        self.error_code = error_code


class NotFoundError(AppException):
    """요청 리소스 미존재 예외 (404)."""

    def __init__(self, detail: str = "요청한 리소스를 찾을 수 없습니다.") -> None:
        super().__init__(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=detail,
            error_code="NOT_FOUND",
        )


class UserNotFoundError(NotFoundError):
    """사용자 조회 실패 예외."""

    def __init__(self, user_id: int) -> None:
        super().__init__(detail=f"ID가 {user_id}인 유저를 찾을 수 없습니다.")


class KioskNotFoundError(NotFoundError):
    """키오스크 디바이스 조회 실패 예외."""

    def __init__(self, bin_id: int) -> None:
        super().__init__(detail=f"ID가 {bin_id}인 키오스크를 찾을 수 없습니다.")


class InsufficientPointsError(AppException):
    """포인트 잔여 한도 부족 예외 (400)."""

    def __init__(self, current_points: int, requested_points: int) -> None:
        super().__init__(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"포인트가 부족합니다. (현재: {current_points}P, 요청: {requested_points}P)",
            error_code="INSUFFICIENT_POINTS",
        )


def register_exception_handlers(app: FastAPI) -> None:
    """FastAPI 전역 에러 핸들러 등록 (외부 Qt/앱 클라이언트와의 detail 포맷 호환 보장)."""

    @app.exception_handler(AppException)
    async def app_exception_handler(
        request: Request, exc: AppException
    ) -> JSONResponse:
        logger.warning(
            f"[Business Error] {request.method} {request.url.path} - {exc.error_code}: {exc.detail}"
        )
        return JSONResponse(
            status_code=exc.status_code,
            content={"detail": exc.detail},
        )

    @app.exception_handler(HTTPException)
    async def http_exception_handler(
        request: Request, exc: HTTPException
    ) -> JSONResponse:
        logger.warning(
            f"[HTTP Error] {request.method} {request.url.path} - {exc.status_code}: {exc.detail}"
        )
        return JSONResponse(
            status_code=exc.status_code,
            content={"detail": exc.detail},
        )

    @app.exception_handler(RequestValidationError)
    async def validation_exception_handler(
        request: Request, exc: RequestValidationError
    ) -> JSONResponse:
        # Pydantic 필드 검증 에러를 단일 가독성 메시지로 병합
        error_msgs = [f"{err['loc'][-1]}: {err['msg']}" for err in exc.errors()]
        detail = "유효성 검증 실패: " + ", ".join(error_msgs)
        logger.warning(
            f"[Validation Error] {request.method} {request.url.path} - {detail}"
        )
        return JSONResponse(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            content={"detail": detail},
        )

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(
        request: Request, exc: Exception
    ) -> JSONResponse:
        # 500 원시 트레이스백 노출을 차단하고 표준 에러 포맷 반환 (exc 인스턴스 직접 전달)
        logger.error(
            f"[Internal Server Error] {request.method} {request.url.path} - {exc}",
            exc_info=exc,
        )
        return JSONResponse(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            content={"detail": "서버 내부 오류가 발생했습니다."},
        )
