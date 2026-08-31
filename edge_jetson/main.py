"""
스마트 분리수거 비전 시스템 - Jetson 최상위 이벤트 루프
"""

import signal
import time

from configs.config import cfg
from core.camera import CameraStream
from core.detector import YOLOv11Detector
from core.trt_engine import TensorRTEngine
from stream.serial_controller import SerialController
from stream.socket_server import StreamSocketServer


def main():
    print("=" * 60)
    print("[EDGE AI] 스마트 분리수거 비전 시스템 부팅 중...")
    print("=" * 60)

    # 1. 하드웨어 및 네트워크 모듈 초기화 (의존성 주입)
    camera = CameraStream(
        src=cfg.cam.src,
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

    # 2. 종료 시그널 핸들링 (Ctrl+C 및 SIGTERM)
    is_running = True

    def handle_signal(sig, frame):
        nonlocal is_running
        print("\n[STOP] 종료 시그널 수신. 메인 루프를 정지합니다...")
        is_running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    print(f"\n[SERVER] 파이프라인 준비 완료. PC 대기 중 -> Port: {cfg.net.port}")
    prev_time = time.time()

    # 3. 메인 이벤트 루프 (순수 try...finally 구조)
    try:
        while is_running:
            # 3-1. 클라이언트 연결 대기 (논블로킹 타임아웃)
            if not socket_server.is_connected:
                socket_server.accept_client()
                continue

            # 3-2. 카메라 최신 프레임 읽기
            ret, frame = camera.read()
            if not ret or frame is None:
                continue

            # 3-3. YOLO AI 추론
            t0 = time.time()
            detections = detector.detect(frame)
            infer_ms = (time.time() - t0) * 1000.0

            # 3-4. STM32 하드웨어 제어 (최고 신뢰도 1건 추출 트리거)
            if detections:
                best_det = max(detections, key=lambda x: x["confidence"])
                serial_ctrl.send_command(class_name=best_det["class_name"])

            # 3-5. 실시간 FPS 계산
            curr_time = time.time()
            time_diff = curr_time - prev_time
            fps = 1.0 / time_diff if time_diff > 0 else 0.0
            prev_time = curr_time

            # 3-6. 관제 PC 메타데이터 송신
            meta = {
                "timestamp": curr_time,
                "fps": round(fps, 1),
                "infer_ms": round(infer_ms, 2),
                "detections": detections,
            }
            socket_server.send_frame(frame, meta)

    finally:
        # 4. 안전한 자원 해제 보장
        print("\n[CLEANUP] 전체 리소스를 안전하게 해제합니다...")
        socket_server.close()
        serial_ctrl.close()

        if hasattr(trt_engine, "destroy"):
            trt_engine.destroy()

        camera.release()
        print("[CLEANUP] 프로그램이 정상적으로 종료되었습니다.")


if __name__ == "__main__":
    main()
