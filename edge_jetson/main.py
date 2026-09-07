"""스마트 재활용 키오스크 엣지 파이프라인 최상위 메인 루프 모듈."""

import signal
import time
from contextlib import suppress

from configs.config import cfg
from core.camera import CameraStream
from core.detector import YOLOv11Detector
from core.door_controller import AutoDoorController
from core.trt_engine import TensorRTEngine
from stream.serial_controller import SerialController
from stream.socket_server import StreamSocketServer
from utils.keyboard import NonBlockingKeyReader


def main():
    """모듈 초기화, AI 객체 검출, 도어 FSM 제어 및 관제 PC 스트리밍 파이프라인 구동."""
    print("=" * 60)
    print("[EDGE AI] 스마트 분리수거 비전 시스템 부팅 중...")
    print("=" * 60)

    # 1. 하드웨어 및 파이프라인 서브시스템 초기화
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

    door_ctrl = AutoDoorController(serial_ctrl=serial_ctrl, config=cfg.door)

    key_reader = NonBlockingKeyReader()
    is_running = True

    # SIGINT(Ctrl+C) 및 SIGTERM 수신 시 안전 종료 플래그 설정
    def handle_signal(sig, frame):
        nonlocal is_running
        print("\n[STOP] 종료 시그널 수신")
        is_running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    print(f"\n[SERVER] 파이프라인 준비 완료 (TCP Port: {cfg.net.port})")
    prev_time = time.time()

    try:
        # 2. 실시간 엣지 파이프라인 메인 루프
        while is_running:
            # 논블로킹 키 입력 감지 ('q' 입력 시 종료)
            key = key_reader.get_key()
            if key and key.lower() == "q":
                break

            # 비차단 클라이언트 접속 폴링
            if not socket_server.is_connected:
                socket_server.accept_client()

            ret, frame = camera.read()
            if not ret or frame is None:
                time.sleep(0.002)  # 프레임 대기 시 CPU 과점유(Busy-wait) 방지
                continue

            # 비전 AI 추론 및 지연시간(Latency) 계측
            t0 = time.time()
            detections = detector.detect(frame)
            infer_ms = (time.time() - t0) * 1000.0

            # 감지 결과 기반 수거함 도어 FSM 상태 전이
            door_ctrl.process_detections(detections)

            # 파이프라인 실효 처리 속도(FPS) 계산
            curr_time = time.time()
            time_diff = curr_time - prev_time
            fps = 1.0 / time_diff if time_diff > 0 else 0.0
            prev_time = curr_time

            # 관제 PC 연결 시에만 JPEG 압축 및 텔레메트리 바이너리 전송 (불필요한 연산 방지)
            if socket_server.is_connected:
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
        # 3. 종료 시 하드웨어 I/O, 네트워크 및 GPU 메모리 안전 일괄 해제
        print("\n[CLEANUP] 전체 리소스를 안전하게 해제합니다...")
        with suppress(Exception):
            key_reader.restore()
        with suppress(Exception):
            socket_server.close()
        with suppress(Exception):
            serial_ctrl.close()
        with suppress(Exception):
            trt_engine.destroy()
        with suppress(Exception):
            camera.release()


if __name__ == "__main__":
    main()
