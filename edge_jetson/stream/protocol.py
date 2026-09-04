"""
stream/protocol.py

지정된 규격 전용 프로토콜 인코더 및 디코더
- MCU -> Jetson: $DOOR_STATE:OPEN/CLOSED, $BIN:PAPER/CAN/PET/VINYL
- Jetson -> MCU: $DOOR_OPEN:<ITEM>, $DOOR_CLOSE
"""

from dataclasses import dataclass
from enum import Enum


class DoorAction(str, Enum):
    OPEN = "OPEN"
    CLOSE = "CLOSE"


class DoorState(str, Enum):
    OPEN = "OPEN"
    CLOSED = "CLOSED"
    UNKNOWN = "UNKNOWN"


@dataclass(frozen=True)
class BinLevels:
    paper: int = 0
    can: int = 0
    pet: int = 0
    vinyl: int = 0

    def to_dict(self) -> dict[str, int]:
        return {
            "paper": self.paper,
            "can": self.can,
            "pet": self.pet,
            "vinyl": self.vinyl,
        }


@dataclass(frozen=True)
class DoorStatus:
    item: str = "ALL"
    state: DoorState = DoorState.CLOSED

    def to_dict(self) -> dict[str, str]:
        return {"item": self.item, "state": self.state.value}


class ProtocolParser:
    """사용자 지정 규격 파서"""

    @classmethod
    def encode_door_command(cls, action: DoorAction, item: str | None = None) -> str:
        """
        Jetson -> MCU 전송 명령 생성
        - 열기: $DOOR_OPEN:PET\n
        - 닫기: $DOOR_CLOSE\n
        """
        if action == DoorAction.OPEN:
            clean_item = (item or "ALL").upper()
            return f"$DOOR_OPEN:{clean_item}\n"
        else:
            return "$DOOR_CLOSE\n"

    @classmethod
    def parse_mcu_line(cls, line: str) -> tuple[str, BinLevels | DoorStatus | None]:
        """
        MCU -> Jetson 수신 라인 파싱
        1) $BIN:45/80/20/10\n (PAPER/CAN/PET/VINYL)
        2) $DOOR_STATE:OPEN\n 또는 $DOOR_STATE:CLOSED\n
        """
        clean_line = line.strip()
        if not clean_line.startswith("$") or ":" not in clean_line:
            return "UNKNOWN", None

        # 콜론(:) 기준으로 헤더와 본문 분리
        header, body = clean_line.split(":", 1)

        # 1. 적재함 잔여량: $BIN:45/80/20/10
        if header == "$BIN":
            levels = body.split("/")
            if len(levels) == 4:
                try:
                    bin_data = BinLevels(
                        paper=int(levels[0]),
                        can=int(levels[1]),
                        pet=int(levels[2]),
                        vinyl=int(levels[3]),
                    )
                    return "BIN", bin_data
                except ValueError:
                    return "ERROR", None

        # 2. 도어 상태: $DOOR_STATE:OPEN / $DOOR_STATE:CLOSED
        elif header == "$DOOR_STATE":
            state_str = body.upper()
            state = DoorState.OPEN if state_str == "OPEN" else DoorState.CLOSED
            return "DOOR", DoorStatus(item="ALL", state=state)

        return "UNKNOWN", None
