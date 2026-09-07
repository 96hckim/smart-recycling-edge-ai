"""비전 감지 결과 기반 디바운스 및 안전 타이머 적용 수거함 도어 FSM 제어 모듈."""

import time
from typing import Any

from configs.config import DoorConfig
from stream.protocol import DoorAction, DoorState
from stream.serial_controller import SerialController


class AutoDoorController:
    """안정 감지 검증 후 개방하고, 최소 유지 시간을 지킨 뒤 닫는 상태 머신 제어기."""

    def __init__(self, serial_ctrl: SerialController, config: DoorConfig):
        """도어 제어 FSM 상태 및 디바운스 카운터 초기화."""
        self.serial_ctrl = serial_ctrl
        self.config = config

        # 상태 머신
        self.current_state: DoorState = DoorState.CLOSED
        self.active_item: str | None = None
        self.door_open_timestamp: float = 0.0

        # 디바운스 및 허용 오차 카운터
        self.candidate_item: str | None = None
        self.consecutive_count: int = 0
        self.lost_count: int = 0

    def process_detections(self, detections: list[dict[str, Any]]) -> None:
        """프레임별 추론 결과를 FSM에 투입하여 도어 상태 전이 및 시리얼 명령 송신."""
        curr_time = time.time()
        top_item = self._extract_top_item(detections)

        if self.current_state == DoorState.CLOSED:
            self._handle_closed_state(top_item, curr_time)
        elif self.current_state == DoorState.OPEN:
            self._handle_open_state(top_item, curr_time)

    def _extract_top_item(self, detections: list[dict[str, Any]]) -> str | None:
        """검출 객체 중 최고 신뢰도 클래스명(대문자) 추출."""
        if not detections:
            return None
        best_det = max(detections, key=lambda x: x.get("confidence", 0.0))
        return best_det.get("class_name", "").upper() or None

    def _handle_closed_state(self, top_item: str | None, curr_time: float) -> None:
        """닫힘 상태: 안정 감지 프레임 도달 시 OPEN 명령 송신 및 상태 전이."""
        if top_item is None:
            self.candidate_item = None
            self.consecutive_count = 0
            return

        if top_item == self.candidate_item:
            self.consecutive_count += 1
        else:
            self.candidate_item = top_item
            self.consecutive_count = 1

        if (
            self.consecutive_count >= self.config.stable_frames
            and self.serial_ctrl.send_command(DoorAction.OPEN, top_item)
        ):
            self.current_state = DoorState.OPEN
            self.active_item = top_item
            self.door_open_timestamp = curr_time
            self.lost_count = 0

    def _handle_open_state(self, top_item: str | None, curr_time: float) -> None:
        """열림 상태: 최소 홀드 시간 보장 및 물체 부재 확인 후 CLOSE 명령 송신 (안전 타임아웃 포함)."""
        elapsed_open = curr_time - self.door_open_timestamp
        is_max_timeout = elapsed_open >= self.config.max_open_sec

        # 최소 홀드 시간 이전에는 닫힘 검사를 유보하고, 최대 타임아웃 초과 시에는 즉시 강제 닫힘 진행
        if not is_max_timeout and elapsed_open < self.config.min_hold_sec:
            return

        if top_item == self.active_item and not is_max_timeout:
            self.lost_count = 0
        else:
            self.lost_count += 1

        # 부재 카운트 초과 또는 모터 보호용 최대 개방 시간 제한 도달 시 도어 폐쇄
        if (
            self.lost_count >= self.config.lost_tolerance or is_max_timeout
        ) and self.serial_ctrl.send_command(DoorAction.CLOSE):
            self.current_state = DoorState.CLOSED
            self.active_item = None
            self.candidate_item = None
            self.consecutive_count = 0
            self.lost_count = 0
