import json
import logging
from typing import Literal

from fastapi import WebSocket, WebSocketDisconnect

logger = logging.getLogger("uvicorn")

ClientType = Literal["kiosk", "mobile"]


class ConnectionManager:
    def __init__(self) -> None:
        # { bin_id: { "kiosk": WebSocket | None, "mobile": WebSocket | None } }
        self._rooms: dict[int, dict[ClientType, WebSocket | None]] = {}

    async def connect(
        self, websocket: WebSocket, bin_id: int, client_type: ClientType
    ) -> None:
        """웹소켓 핸드셰이크 승인 및 기기 룸에 등록."""
        await websocket.accept()

        if bin_id not in self._rooms:
            self._rooms[bin_id] = {"kiosk": None, "mobile": None}

        # 기존 연결이 남아있다면 덮어쓰기
        self._rooms[bin_id][client_type] = websocket
        logger.info(f"[WS Connect] Room {bin_id} - {client_type} 연결됨")

    def disconnect(self, bin_id: int, client_type: ClientType) -> None:
        """소켓 연결 종료 시 룸에서 제거 및 빈 방 메모리 정리."""
        if bin_id in self._rooms:
            self._rooms[bin_id][client_type] = None
            logger.info(f"[WS Disconnect] Room {bin_id} - {client_type} 해제됨")

            # 양쪽 기기 모두 연결이 끊겼으면 룸 삭제
            if (
                self._rooms[bin_id]["kiosk"] is None
                and self._rooms[bin_id]["mobile"] is None
            ):
                del self._rooms[bin_id]
                logger.info(f"[WS Room Deleted] Room {bin_id} 삭제됨")

    async def send_to_kiosk(self, bin_id: int, event_data: dict) -> bool:
        """특정 키오스크로 단독 이벤트 발송 (예: USER_AUTHENTICATED)."""
        room = self._rooms.get(bin_id)
        if room and room["kiosk"]:
            try:
                await room["kiosk"].send_text(
                    json.dumps(event_data, ensure_ascii=False)
                )
                logger.info(
                    f"[WS Send -> Kiosk] Room {bin_id}: {event_data.get('event')}"
                )
                return True
            except (WebSocketDisconnect, RuntimeError) as e:
                logger.warning(f"[WS Send Failed -> Kiosk] Room {bin_id}: {e}")
                self.disconnect(bin_id, "kiosk")
        return False

    async def send_to_mobile(self, bin_id: int, event_data: dict) -> bool:
        """특정 모바일 앱으로 단독 이벤트 발송 (예: RECYCLE_COMPLETE)."""
        room = self._rooms.get(bin_id)
        if room and room["mobile"]:
            try:
                await room["mobile"].send_text(
                    json.dumps(event_data, ensure_ascii=False)
                )
                logger.info(
                    f"[WS Send -> Mobile] Room {bin_id}: {event_data.get('event')}"
                )
                return True
            except (WebSocketDisconnect, RuntimeError) as e:
                logger.warning(f"[WS Send Failed -> Mobile] Room {bin_id}: {e}")
                self.disconnect(bin_id, "mobile")
        return False


# 서버 전역에서 단일 인스턴스로 사용
manager = ConnectionManager()
