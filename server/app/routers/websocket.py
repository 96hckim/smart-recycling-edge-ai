"""키오스크(Qt) 및 모바일 클라이언트 실시간 전이 이벤트 WebSocket 라우터 모듈."""

import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.connection_manager import ClientType, manager

logger = logging.getLogger("uvicorn")

router = APIRouter(tags=["WebSocket"])


@router.websocket("/ws/kiosk/{bin_id}/{client_type}")
async def websocket_kiosk_endpoint(
    websocket: WebSocket,
    bin_id: int,
    client_type: str,
) -> None:
    """키오스크 및 모바일 디바이스 1:1 룸 연결, 이벤트 루프 폴링 및 세션 종료 처리."""
    # 1. 비인가 클라이언트 타입 차단 (WS 1008 Policy Violation 종료)
    if client_type not in ("kiosk", "mobile"):
        await websocket.close(
            code=1008, reason="유효하지 않은 client_type (kiosk 또는 mobile 필수)"
        )
        return

    valid_client_type: ClientType = "kiosk" if client_type == "kiosk" else "mobile"

    # 2. 룸 등록 및 기존 좀비 연결 정리
    await manager.connect(websocket, bin_id, valid_client_type)

    try:
        # 하트비트/핑퐁 및 클라이언트 메시지 수신 대기 루프
        while True:
            data = await websocket.receive_text()
            logger.debug(f"[WS Recv] Room {bin_id} ({valid_client_type}): {data}")
    except WebSocketDisconnect:
        logger.info(
            f"[WS Client Disconnected] Room {bin_id} ({valid_client_type}) 정상 종료"
        )
    except (RuntimeError, OSError) as e:
        logger.warning(
            f"[WS Connection Error] Room {bin_id} ({valid_client_type}): {e}"
        )

    finally:
        # 네트워크 끊김 및 비정상 종료 시에도 현재 소켓 인스턴스 검증 기반 룸 자원 안전 해제
        await manager.disconnect(bin_id, valid_client_type, websocket=websocket)
