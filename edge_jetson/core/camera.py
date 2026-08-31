"""
edge_jetson/core/camera.py

초저지연 백그라운드 프레임 캡처 모듈 (USB V4L2 및 Jetson CSI 지원)
"""

import threading
import time

import cv2
import numpy as np


def build_gstreamer_pipeline(
    sensor_id: int = 0,
    capture_width: int = 1280,
    capture_height: int = 720,
    display_width: int = 640,
    display_height: int = 480,
    framerate: int = 60,
    flip_method: int = 0,
) -> str:
    """Jetson 하드웨어 ISP 가속 CSI 카메라(nvarguscamerasrc) 파이프라인 생성"""
    return (
        f"nvarguscamerasrc sensor-id={sensor_id} ! "
        f"video/x-raw(memory:NVMM), width=(int){capture_width}, height=(int){capture_height}, "
        f"format=(string)NV12, framerate=(fraction){framerate}/1 ! "
        f"nvvidconv flip-method={flip_method} ! "
        f"video/x-raw, width=(int){display_width}, height=(int){display_height}, format=(string)BGRx ! "
        f"videoconvert ! "
        f"video/x-raw, format=(string)BGR ! appsink drop=true sync=false"
    )


class CameraStream:
    """
    백그라운드 전용 스레드에서 최신 프레임을 지속적으로 갱신하여
    버퍼 지연을 원천 차단하는 초저지연 카메라 스트림 클래스
    """

    def __init__(
        self,
        src: int | str = 0,
        width: int = 640,
        height: int = 480,
        fps: int = 60,
        buffer_size: int = 1,
        flip_horizontal: bool = True,
    ):
        self.src = src
        self.width = width
        self.height = height
        self.fps = fps
        self.flip_horizontal = flip_horizontal

        # 1. 카메라 캡처 객체 생성 (CSI 문자열 또는 V4L2 인덱스)
        if isinstance(src, str) and src.startswith("csi:"):
            sensor_id = int(src.split(":")[1]) if len(src.split(":")) > 1 else 0
            pipeline = build_gstreamer_pipeline(
                sensor_id=sensor_id,
                display_width=width,
                display_height=height,
                framerate=fps,
            )
            print(f"[CAMERA] CSI 카메라 파이프라인 구동: sensor_id={sensor_id}")
            self.cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
        else:
            self.cap = cv2.VideoCapture(src)
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
            self.cap.set(cv2.CAP_PROP_FPS, fps)
            self.cap.set(cv2.CAP_PROP_BUFFERSIZE, buffer_size)

        if not self.cap.isOpened():
            raise RuntimeError(
                f"[CAMERA ERROR] 카메라 디바이스({src})를 열 수 없습니다."
            )

        # 첫 프레임 동기 확인
        self.ret, self.frame = self.cap.read()
        if not self.ret or self.frame is None:
            raise RuntimeError(
                f"[CAMERA ERROR] 카메라({src})로부터 초기 프레임을 읽지 못했습니다."
            )

        # 2. 백그라운드 캡처 스레드 구동
        self.running = True
        self.lock = threading.Lock()
        self.thread = threading.Thread(
            target=self._capture_loop, name="CameraWorker", daemon=True
        )
        self.thread.start()
        print(f"[CAMERA] 백그라운드 캡처 스레드 시작 ({width}x{height} @ {fps}fps)")

    def _capture_loop(self):
        """백그라운드에서 최신 프레임을 실시간으로 갱신하는 루프"""
        while self.running:
            ret, frame = self.cap.read()
            if ret and frame is not None:
                # 좌우 반전 적용 (1: 좌우 반전, 0: 상하 반전, -1: 상하좌우 반전)
                if self.flip_horizontal:
                    frame = cv2.flip(frame, 1)

                with self.lock:
                    self.frame = frame
                    self.ret = ret
            else:
                # 프레임 드랍 발생 시 짧은 슬립으로 CPU 과열 방지
                time.sleep(0.005)

    def read(self) -> tuple[bool, np.ndarray | None]:
        """최신 프레임 참조를 스레드 안전하게 반환 (복사 오버헤드 0ms)"""
        with self.lock:
            if not self.ret or self.frame is None:
                return False, None
            return True, self.frame

    def release(self):
        """스레드 정지 및 하드웨어 장치 안전 해제"""
        self.running = False
        if hasattr(self, "thread") and self.thread.is_alive():
            self.thread.join(timeout=1.0)

        if hasattr(self, "cap") and self.cap.isOpened():
            self.cap.release()
            print("[CAMERA] 디바이스 해제 완료")

    def __del__(self):
        self.release()
