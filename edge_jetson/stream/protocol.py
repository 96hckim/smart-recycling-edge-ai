"""Jetson-STM32 간 UART 통신 ASCII 프로토콜 인코딩/디코딩 모듈."""

from dataclasses import asdict, dataclass
from enum import Enum

# 기본 도어 제어 품목명 상수 (전체 공통 사용)
DEFAULT_DOOR_ITEM = "ALL"


class DoorAction(str, Enum):
    """도어 제어 액션 규격 (OPEN / CLOSE)."""

    OPEN = "OPEN"
    CLOSE = "CLOSE"


class DoorState(str, Enum):
    """MCU 리미트 센서 기반 도어 물리 상태."""

    OPEN = "OPEN"
    CLOSED = "CLOSED"
    UNKNOWN = "UNKNOWN"


class McuHeader(str, Enum):
    """STM32 수신 패킷 헤더 규격."""

    BIN = "$BIN"
    DOOR_STATE = "$DOOR_STATE"


class McuPacketType(str, Enum):
    """파싱된 MCU 패킷 유형 식별자."""

    BIN = "BIN"
    DOOR = "DOOR"
    UNKNOWN = "UNKNOWN"
    ERROR = "ERROR"


# STM32 수위 전송 필드 순서 규격 (paper / can / pet / vinyl)
BIN_FIELD_NAMES: tuple[str, ...] = ("paper", "can", "pet", "vinyl")


@dataclass(frozen=True)
class BinLevels:
    """수거함 4개 구역 적재율(%) 데이터 모델."""

    paper: int = 0
    can: int = 0
    pet: int = 0
    vinyl: int = 0

    def to_dict(self) -> dict[str, int]:
        """관제 PC 대시보드 전송용 딕셔너리 변환."""
        return asdict(self)


@dataclass(frozen=True)
class DoorStatus:
    """도어 현재 물리 상태 및 제어 품목 정보 모델."""

    item: str = DEFAULT_DOOR_ITEM
    state: DoorState = DoorState.CLOSED

    def to_dict(self) -> dict[str, str]:
        """관제 PC 대시보드 전송용 딕셔너리 변환."""
        return {"item": self.item, "state": self.state.value}


class ProtocolParser:
    """UART 패킷 인코딩 및 디코딩 유틸리티 클래스."""

    DELIM_START = "$"
    DELIM_HEADER = ":"
    DELIM_VALUE = "/"
    LINE_END = "\n"

    CMD_DOOR_OPEN = "$DOOR_OPEN"
    CMD_DOOR_CLOSE = f"$DOOR_CLOSE{LINE_END}"

    @classmethod
    def encode_door_command(cls, action: DoorAction, item: str | None = None) -> str:
        """도어 제어 액션을 MCU 규격 문자열 패킷($DOOR_OPEN:<ITEM>\n, $DOOR_CLOSE\n)으로 인코딩."""
        if action == DoorAction.OPEN:
            clean_item = (item or DEFAULT_DOOR_ITEM).upper()
            return f"{cls.CMD_DOOR_OPEN}:{clean_item}{cls.LINE_END}"
        return cls.CMD_DOOR_CLOSE

    @classmethod
    def parse_mcu_line(
        cls, line: str
    ) -> tuple[McuPacketType, BinLevels | DoorStatus | None]:
        """수신된 1줄의 ASCII 문자열($BIN, $DOOR_STATE)을 파싱하여 상태 객체로 변환."""
        clean_line = line.strip()
        if (
            not clean_line.startswith(cls.DELIM_START)
            or cls.DELIM_HEADER not in clean_line
        ):
            return McuPacketType.UNKNOWN, None

        header, body = clean_line.split(cls.DELIM_HEADER, 1)
        header = header.strip()
        body = body.strip()

        if header == McuHeader.BIN.value:
            levels = body.split(cls.DELIM_VALUE)
            if len(levels) == len(BIN_FIELD_NAMES):
                try:
                    kwargs = {
                        field: int(val.strip())
                        for field, val in zip(BIN_FIELD_NAMES, levels)
                    }
                    return McuPacketType.BIN, BinLevels(**kwargs)
                except ValueError:
                    return McuPacketType.ERROR, None

        elif header == McuHeader.DOOR_STATE.value:
            try:
                state = DoorState(body.upper())
                return (
                    McuPacketType.DOOR,
                    DoorStatus(item=DEFAULT_DOOR_ITEM, state=state),
                )
            except ValueError:
                return McuPacketType.ERROR, None

        return McuPacketType.UNKNOWN, None
