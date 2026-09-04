"""
edge_jetson/main.py

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
from utils.keyboard import NonBlockingKeyReader


def main():
    print("=" * 60)
    print("[EDGE AI] 스마트 분리수거 비전 시스템 부팅 중...")
    print("=" * 60)

    # 1. 하드웨어 및 통신 모듈 초기화 (설정값 주입)
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

    # 2. 비차단 키보드 리더 및 OS 시그널 핸들러 등록
    key_reader = NonBlockingKeyReader()
    is_running = True

    def handle_signal(sig, frame):
        nonlocal is_running
        print("\n[STOP] OS 종료 시그널 수신 -> 메인 루프 정지")
        is_running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    # 3. 오인식 방지용 Debounce 필터 변수
    STABLE_FRAME_THRESHOLD = 5  # 동일 클래스 연속 5프레임 검출 시 트리거
    current_target_class: str | None = None
    consecutive_detected_count = 0

    print(f"\n[SERVER] 파이프라인 준비 완료 (TCP Port: {cfg.net.port})")
    print("[INFO] 터미널에서 'q' 키를 누르면 엔터 없이 즉시 안전하게 종료됩니다.\n")

    prev_time = time.time()

    # 4. 메인 이벤트 루프
    try:
        while is_running:
            # 4-1. 단일 키 'q' 입력 감지 (즉시 탈출)
            key = key_reader.get_key()
            if key and key.lower() == "q":
                print("\n[SYSTEM] 사용자 'q' 입력 감지 -> 정상 종료 시퀀스 시작")
                break

            # 4-2. 관제 PC 대시보드 연결 대기 (논블로킹)
            if not socket_server.is_connected:
                socket_server.accept_client()
                continue

            # 4-3. 카메라 최신 프레임 획득 (초저지연)
            ret, frame = camera.read()
            if not ret or frame is None:
                continue

            # 4-4. TensorRT AI 추론
            t0 = time.time()
            detections = detector.detect(frame)
            infer_ms = (time.time() - t0) * 1000.0

            # 4-5. STM32 하드웨어 제어 (오인식 방지 필터링)
            if detections:
                best_det = max(detections, key=lambda x: x["confidence"])
                detected_name = best_det["class_name"]

                if detected_name == current_target_class:
                    consecutive_detected_count += 1
                else:
                    current_target_class = detected_name
                    consecutive_detected_count = 1

                # 연속 검출 기준 충족 시 STM32 명령 전송 (쿨다운 2초 내부 관리)
                if consecutive_detected_count >= STABLE_FRAME_THRESHOLD:
                    serial_ctrl.send_command(class_name=detected_name)
            else:
                current_target_class = None
                consecutive_detected_count = 0

            # 4-6. 실시간 FPS 계산
            curr_time = time.time()
            time_diff = curr_time - prev_time
            fps = 1.0 / time_diff if time_diff > 0 else 0.0
            prev_time = curr_time

            # 4-7. 관제 PC로 [영상 + 메타데이터] 일괄 패킷 송신
            meta = {
                "timestamp": curr_time,
                "fps": round(fps, 1),
                "infer_ms": round(infer_ms, 2),
                "detections": detections,
            }
            socket_server.send_frame(frame, meta)

    finally:
        # 5. 모든 자원 안전 반환 (역순 정리)
        print("\n[CLEANUP] 전체 리소스를 안전하게 해제합니다...")
        key_reader.restore()
        socket_server.close()
        serial_ctrl.close()
        trt_engine.destroy()
        camera.release()
        print(
            f"[CLEANUP] 포트 {cfg.net.port} 및 모든 하드웨어 자원이 정상 반환되었습니다."
        )


if __name__ == "__main__":
    main()
