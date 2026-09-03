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
    # 1. 클라이언트 식별자 검증
    if client_type not in ("kiosk", "mobile"):
        await websocket.close(
            code=1008, reason="유효하지 않은 client_type (kiosk 또는 mobile 필수)"
        )
        return

    valid_client_type: ClientType = "kiosk" if client_type == "kiosk" else "mobile"

    # 2. 커넥션 매니저 등록
    await manager.connect(websocket, bin_id, valid_client_type)

    try:
        # 클라이언트 연결 유지 및 수신 대기 (핑퐁 / 하트비트 대응)
        while True:
            # 양방향 통신이 필요할 경우 수신 메시지를 처리할 수 있음
            data = await websocket.receive_text()
            logger.debug(f"[WS Recv] Room {bin_id} ({valid_client_type}): {data}")
    except WebSocketDisconnect:
        logger.info(
            f"[WS Client Disconnected] Room {bin_id} ({valid_client_type}) 정상 종료"
        )
        manager.disconnect(bin_id, valid_client_type)
    except RuntimeError as e:
        logger.warning(
            f"[WS Connection Error] Room {bin_id} ({valid_client_type}): {e}"
        )
        manager.disconnect(bin_id, valid_client_type)
