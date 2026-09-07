"""키오스크와 모바일 간 1:1 룸 기반 실시간 WebSocket 연결 관리 모듈."""

import asyncio
import json
import logging
from typing import Literal

from fastapi import WebSocket, WebSocketDisconnect

logger = logging.getLogger("uvicorn")

ClientType = Literal["kiosk", "mobile"]


class ConnectionManager:
    """기기별 1:1 룸 매핑, 좀비 세션 방어 및 스레드 안전 비동기 메시지 디스패처."""

    def __init__(self) -> None:
        # { bin_id: { "kiosk": WebSocket | None, "mobile": WebSocket | None } }
        self._rooms: dict[int, dict[ClientType, WebSocket | None]] = {}
        self._lock = asyncio.Lock()

    async def connect(
        self, websocket: WebSocket, bin_id: int, client_type: ClientType
    ) -> None:
        """웹소켓 연결 수락 및 룸 등록 (동일 기기 재접속 시 이전 소켓 명시적 종료로 누수 방지)."""
        await websocket.accept()

        async with self._lock:
            if bin_id not in self._rooms:
                self._rooms[bin_id] = {"kiosk": None, "mobile": None}

            # 재연결 시 잔존하는 기존 좀비 소켓 정상 종료
            old_socket = self._rooms[bin_id].get(client_type)
            if old_socket is not None and old_socket != websocket:
                logger.info(
                    f"[WS Connect] Room {bin_id} - 기존 {client_type} 소켓 세션 명시적 종료"
                )
                try:
                    await old_socket.close(
                        code=1000, reason="Superceded by new connection"
                    )
                except (WebSocketDisconnect, RuntimeError, OSError) as e:
                    logger.debug(
                        f"[WS Connect] 기존 세션 종료 중 예외 (무시 가능): {e}"
                    )

            self._rooms[bin_id][client_type] = websocket
            logger.info(f"[WS Connect] Room {bin_id} - {client_type} 연결 완료")

    async def disconnect(
        self,
        bin_id: int,
        client_type: ClientType,
        websocket: WebSocket | None = None,
    ) -> None:
        """소켓 종료 시 룸에서 등록 해제하고, 양측 기기 모두 부재 시 룸 메모리 정리."""
        async with self._lock:
            if bin_id in self._rooms:
                current_socket = self._rooms[bin_id].get(client_type)
                # 재연결 레이스 컨디션으로 인해 이미 새 소켓으로 대체된 경우 해제 무시
                if websocket is not None and current_socket != websocket:
                    logger.info(
                        f"[WS Disconnect Ignored] Room {bin_id} - {client_type}는 이미 새로운 연결로 교체됨"
                    )
                    return

                self._rooms[bin_id][client_type] = None
                logger.info(f"[WS Disconnect] Room {bin_id} - {client_type} 해제됨")

                # 활성 연결이 없는 빈 룸 딕셔너리 메모리 해제
                if (
                    self._rooms[bin_id]["kiosk"] is None
                    and self._rooms[bin_id]["mobile"] is None
                ):
                    del self._rooms[bin_id]
                    logger.info(f"[WS Room Deleted] Room {bin_id} 삭제됨")

    async def send_to_kiosk(self, bin_id: int, event_data: dict) -> bool:
        """지정 키오스크 소켓으로 이벤트 단독 송신 (전송 실패 시 세션 자동 정리)."""
        target_ws: WebSocket | None = None
        async with self._lock:
            room = self._rooms.get(bin_id)
            if room:
                target_ws = room.get("kiosk")

        if target_ws is not None:
            try:
                await target_ws.send_text(json.dumps(event_data, ensure_ascii=False))
                logger.info(
                    f"[WS Send -> Kiosk] Room {bin_id}: {event_data.get('event')}"
                )
                return True
            except (WebSocketDisconnect, RuntimeError, OSError) as e:
                logger.warning(f"[WS Send Failed -> Kiosk] Room {bin_id}: {e}")
                await self.disconnect(bin_id, "kiosk", websocket=target_ws)
        return False

    async def send_to_mobile(self, bin_id: int, event_data: dict) -> bool:
        """지정 모바일 소켓으로 이벤트 단독 송신 (전송 실패 시 세션 자동 정리)."""
        target_ws: WebSocket | None = None
        async with self._lock:
            room = self._rooms.get(bin_id)
            if room:
                target_ws = room.get("mobile")

        if target_ws is not None:
            try:
                await target_ws.send_text(json.dumps(event_data, ensure_ascii=False))
                logger.info(
                    f"[WS Send -> Mobile] Room {bin_id}: {event_data.get('event')}"
                )
                return True
            except (WebSocketDisconnect, RuntimeError, OSError) as e:
                logger.warning(f"[WS Send Failed -> Mobile] Room {bin_id}: {e}")
                await self.disconnect(bin_id, "mobile", websocket=target_ws)
        return False


# 서버 전역 싱글톤 인스턴스
manager = ConnectionManager()
