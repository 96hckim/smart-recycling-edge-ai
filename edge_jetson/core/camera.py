"""
core/camera.py

초저지연 백그라운드 카메라 프레임 캡처 모듈
"""

import threading
import time

import cv2
import numpy as np


class CameraStream:
    """백그라운드 스레드에서 최신 프레임을 실시간 갱신하는 캡처 클래스"""

    def __init__(
        self,
        device_id: int = 0,
        width: int = 640,
        height: int = 480,
        fps: int = 60,
        buffer_size: int = 1,
        flip_horizontal: bool = True,
    ):
        self.device_id = device_id
        self.width = width
        self.height = height
        self.fps = fps
        self.flip_horizontal = flip_horizontal

        self.cap: cv2.VideoCapture | None = None
        self.thread: threading.Thread | None = None
        self.frame: np.ndarray | None = None
        self.ret: bool = False
        self.running: bool = False
        self.lock: threading.Lock = threading.Lock()

        # 1. 카메라 디바이스 초기화
        self.cap = cv2.VideoCapture(device_id, cv2.CAP_V4L2)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self.cap.set(cv2.CAP_PROP_FPS, fps)
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, buffer_size)

        if not self.cap.isOpened():
            raise RuntimeError(
                f"[CAMERA ERROR] 카메라 장치({device_id})를 열 수 없습니다."
            )

        # 첫 프레임 수신 테스트
        self.ret, self.frame = self.cap.read()
        if not self.ret or self.frame is None:
            self.release()
            raise RuntimeError(
                f"[CAMERA ERROR] 카메라({device_id}) 초기 프레임 획득 실패"
            )

        # 2. 백그라운드 캡처 스레드 시작
        self.running = True
        self.thread = threading.Thread(
            target=self._capture_loop, name="CameraWorker", daemon=True
        )
        self.thread.start()
        print(f"[CAMERA] 백그라운드 캡처 시작 ({width}x{height} @ {fps}fps)")

    def _capture_loop(self):
        """백그라운드에서 최신 프레임을 주기적으로 갱신"""
        while self.running:
            if self.cap is None:
                break

            ret, frame = self.cap.read()
            if ret and frame is not None:
                if self.flip_horizontal:
                    frame = cv2.flip(frame, 1)

                with self.lock:
                    self.frame = frame
                    self.ret = ret
            else:
                time.sleep(0.005)

    def read(self) -> tuple[bool, np.ndarray | None]:
        """최신 프레임 참조 반환 (스레드 동기화 보장)"""
        with self.lock:
            if not self.ret or self.frame is None:
                return False, None
            return True, self.frame

    def release(self):
        """스레드 종료 및 카메라 장치 해제 (중복 호출 안전)"""
        self.running = False

        if self.thread is not None and self.thread.is_alive():
            self.thread.join(timeout=1.0)
            self.thread = None

        if self.cap is not None:
            if self.cap.isOpened():
                self.cap.release()
                print("[CAMERA] 장치 해제 완료")
            self.cap = None

    def __del__(self):
        self.release()
