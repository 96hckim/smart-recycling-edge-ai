"""Jetson-STM32 간 UART 통신 ASCII 프로토콜 인코딩/디코딩 모듈."""

from dataclasses import dataclass
from enum import Enum


class DoorAction(str, Enum):
    """도어 제어 액션 규격 (OPEN / CLOSE)."""

    OPEN = "OPEN"
    CLOSE = "CLOSE"


class DoorState(str, Enum):
    """MCU 리미트 센서 기반 도어 물리 상태."""

    OPEN = "OPEN"
    CLOSED = "CLOSED"
    UNKNOWN = "UNKNOWN"


@dataclass(frozen=True)
class BinLevels:
    """수거함 4개 구역 적재율(%) 데이터 모델."""

    paper: int = 0
    can: int = 0
    pet: int = 0
    vinyl: int = 0

    def to_dict(self) -> dict[str, int]:
        """관제 PC 대시보드 전송용 딕셔너리 변환."""
        return {
            "paper": self.paper,
            "can": self.can,
            "pet": self.pet,
            "vinyl": self.vinyl,
        }


@dataclass(frozen=True)
class DoorStatus:
    """도어 현재 물리 상태 및 제어 품목 정보 모델."""

    item: str = "ALL"
    state: DoorState = DoorState.CLOSED

    def to_dict(self) -> dict[str, str]:
        """관제 PC 대시보드 전송용 딕셔너리 변환."""
        return {"item": self.item, "state": self.state.value}


class ProtocolParser:
    """UART 패킷 인코딩 및 디코딩 유틸리티 클래스."""

    @classmethod
    def encode_door_command(cls, action: DoorAction, item: str | None = None) -> str:
        """도어 제어 액션을 MCU 규격 문자열 패킷($DOOR_OPEN:<ITEM>\\n, $DOOR_CLOSE\\n)으로 인코딩."""
        if action == DoorAction.OPEN:
            clean_item = (item or "ALL").upper()
            return f"$DOOR_OPEN:{clean_item}\n"
        else:
            return "$DOOR_CLOSE\n"

    @classmethod
    def parse_mcu_line(cls, line: str) -> tuple[str, BinLevels | DoorStatus | None]:
        """수신된 1줄의 ASCII 문자열($BIN, $DOOR_STATE)을 파싱하여 상태 객체로 변환."""
        clean_line = line.strip()
        if not clean_line.startswith("$") or ":" not in clean_line:
            return "UNKNOWN", None

        header, body = clean_line.split(":", 1)
        header = header.strip()
        body = body.strip()

        if header == "$BIN":
            levels = body.split("/")
            if len(levels) == 4:
                try:
                    bin_data = BinLevels(
                        paper=int(levels[0].strip()),
                        can=int(levels[1].strip()),
                        pet=int(levels[2].strip()),
                        vinyl=int(levels[3].strip()),
                    )
                    return "BIN", bin_data
                except ValueError:
                    return "ERROR", None

        elif header == "$DOOR_STATE":
            state_str = body.upper()
            if state_str == "OPEN":
                return "DOOR", DoorStatus(item="ALL", state=DoorState.OPEN)
            elif state_str == "CLOSED":
                return "DOOR", DoorStatus(item="ALL", state=DoorState.CLOSED)
            return "ERROR", None

        return "UNKNOWN", None
