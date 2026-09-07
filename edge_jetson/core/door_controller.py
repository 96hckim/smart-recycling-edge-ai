"""
core/door_controller.py

비전 감지 결과를 기반으로 디바운스 및 최소 유지 시간을 보장하는 도어 FSM 제어기
"""

import time
from typing import Any

from configs.config import DoorConfig
from stream.protocol import DoorAction, DoorState
from stream.serial_controller import SerialController


class AutoDoorController:
    """안정 감지 검증 후 개방하고, 최소 유지 시간을 지킨 뒤 닫는 상태 머신"""

    def __init__(self, serial_ctrl: SerialController, config: DoorConfig):
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
        """매 프레임 추론 결과를 FSM에 투입하여 안정적인 상태 전이 실행"""
        curr_time = time.time()
        top_item = self._extract_top_item(detections)

        if self.current_state == DoorState.CLOSED:
            self._handle_closed_state(top_item, curr_time)
        elif self.current_state == DoorState.OPEN:
            self._handle_open_state(top_item, curr_time)

    def _extract_top_item(self, detections: list[dict[str, Any]]) -> str | None:
        """가장 신뢰도가 높은 클래스명 추출"""
        if not detections:
            return None
        best_det = max(detections, key=lambda x: x.get("confidence", 0.0))
        return best_det.get("class_name", "").upper() or None

    def _handle_closed_state(self, top_item: str | None, curr_time: float) -> None:
        """문이 닫힌 상태: 연속 안정 감지 시 OPEN 전송"""
        if top_item is None:
            self.candidate_item = None
            self.consecutive_count = 0
            return

        if top_item == self.candidate_item:
            self.consecutive_count += 1
        else:
            self.candidate_item = top_item
            self.consecutive_count = 1

        # 중첩 if 제거: 카운트 달성 및 시리얼 전송 성공 조건을 and로 병합
        if (
            self.consecutive_count >= self.config.stable_frames
            and self.serial_ctrl.send_command(DoorAction.OPEN, top_item)
        ):
            self.current_state = DoorState.OPEN
            self.active_item = top_item
            self.door_open_timestamp = curr_time
            self.lost_count = 0

    def _handle_open_state(self, top_item: str | None, curr_time: float) -> None:
        """문이 열린 상태: 최소 시간 보장 및 물체 부재 확인 후 CLOSE 전송"""
        # 최소 홀드 시간이 지나기 전에는 닫힘 검사 유보
        if (curr_time - self.door_open_timestamp) < self.config.min_hold_sec:
            return

        if top_item == self.active_item:
            self.lost_count = 0
        else:
            self.lost_count += 1

        # 중첩 if 제거: 허용 오차 초과 및 시리얼 전송 성공 조건을 and로 병합
        if (
            self.lost_count >= self.config.lost_tolerance
            and self.serial_ctrl.send_command(DoorAction.CLOSE)
        ):
            self.current_state = DoorState.CLOSED
            self.active_item = None
            self.candidate_item = None
            self.consecutive_count = 0
            self.lost_count = 0
