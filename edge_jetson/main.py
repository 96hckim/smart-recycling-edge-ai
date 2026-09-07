"""
edge_jetson/main.py

스마트 분리수거 비전 시스템 - Jetson 최상위 이벤트 루프
"""

import signal
import time

from configs.config import cfg
from core.camera import CameraStream
from core.detector import YOLOv11Detector
from core.door_controller import AutoDoorController
from core.trt_engine import TensorRTEngine
from stream.serial_controller import SerialController
from stream.socket_server import StreamSocketServer
from utils.keyboard import NonBlockingKeyReader


def main():
    print("=" * 60)
    print("[EDGE AI] 스마트 분리수거 비전 시스템 부팅 중...")
    print("=" * 60)

    # 1. 모듈 초기화
    camera = CameraStream(
        device_id=cfg.cam.device_id,
        width=cfg.cam.width,
        height=cfg.cam.height,
        fps=cfg.cam.fps,
        buffer_size=cfg.cam.buffer_size,
        flip_horizontal=cfg.cam.flip_horizontal,
    )

    trt_engine = TensorRTEngine(engine_path=cfg.model.engine_path)

    detector = YOLOv11Detector(
        engine=trt_engine,
        input_shape=cfg.model.input_shape,
        conf_thresh=cfg.model.conf_threshold,
        iou_thresh=cfg.model.iou_threshold,
        class_names=cfg.model.class_names,
    )

    socket_server = StreamSocketServer(
        host=cfg.net.host,
        port=cfg.net.port,
        jpeg_quality=cfg.net.jpeg_quality,
        timeout=cfg.net.socket_timeout,
    )

    serial_ctrl = SerialController(
        port=cfg.serial.port,
        baudrate=cfg.serial.baudrate,
        timeout=cfg.serial.timeout,
        enabled=cfg.serial.enabled,
    )

    # 도어 자동 제어기 초기화
    door_ctrl = AutoDoorController(serial_ctrl=serial_ctrl, config=cfg.door)

    # 2. 비차단 입력 및 시그널 핸들러
    key_reader = NonBlockingKeyReader()
    is_running = True

    def handle_signal(sig, frame):
        nonlocal is_running
        print("\n[STOP] 종료 시그널 수신")
        is_running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    print(f"\n[SERVER] 파이프라인 준비 완료 (TCP Port: {cfg.net.port})")
    prev_time = time.time()

    # 3. 메인 이벤트 루프
    try:
        while is_running:
            # 3-1. 종료 키 확인
            key = key_reader.get_key()
            if key and key.lower() == "q":
                break

            # 3-2. 관제 PC 대시보드 연결 대기 (논블로킹)
            if not socket_server.is_connected:
                socket_server.accept_client()
                continue

            # 3-3. 프레임 캡처
            ret, frame = camera.read()
            if not ret or frame is None:
                continue

            # 3-4. AI 추론 실행
            t0 = time.time()
            detections = detector.detect(frame)
            infer_ms = (time.time() - t0) * 1000.0

            # 3-5. 도어 개폐 비즈니스 로직 위임 처리
            door_ctrl.process_detections(detections)

            # 3-6. FPS 계산
            curr_time = time.time()
            time_diff = curr_time - prev_time
            fps = 1.0 / time_diff if time_diff > 0 else 0.0
            prev_time = curr_time

            # 3-7. 최신 하드웨어 상태 수집 및 관제 클라이언트 전송
            bin_levels, door_status = serial_ctrl.get_latest_data()
            meta = {
                "timestamp": curr_time,
                "fps": round(fps, 1),
                "infer_ms": round(infer_ms, 2),
                "detections": detections,
                "bin_levels": bin_levels,
                "door": door_status,
            }
            socket_server.send_frame(frame, meta)

    finally:
        print("\n[CLEANUP] 전체 리소스를 안전하게 해제합니다...")
        key_reader.restore()
        socket_server.close()
        serial_ctrl.close()
        trt_engine.destroy()
        camera.release()


if __name__ == "__main__":
    main()
