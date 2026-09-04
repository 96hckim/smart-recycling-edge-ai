"""
edge_jetson/main.py

스마트 분리수거 비전 시스템 - Jetson 최상위 이벤트 루프
(Qt 화면 세션에 따른 온디맨드 스트리밍 + 자동 객체 인식 도어 개방)
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

    # 1. 하드웨어 및 통신 모듈 초기화
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

    # 2. 비차단 키보드 리더 및 OS 시그널 등록
    key_reader = NonBlockingKeyReader()
    is_running = True

    def handle_signal(sig, frame):
        nonlocal is_running
        print("\n[STOP] OS 종료 시그널 수신 -> 메인 루프 정지")
        is_running = False

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    # 3. 세션 제어 및 오토 디텍트 상태 변수
    is_streaming = False
    current_target_class: str | None = None
    consecutive_detected_count = 0

    def reset_detection_state():
        nonlocal current_target_class, consecutive_detected_count
        current_target_class = None
        consecutive_detected_count = 0

    # 4. Qt 제어 명령 디스패처 (세션 시작/종료 전용)
    def handle_start_stream(_: dict):
        nonlocal is_streaming
        is_streaming = True
        reset_detection_state()
        print("[CTRL] 분리배출 세션 시작 -> AI 추론 및 영상 스트리밍 활성화")

    def handle_stop_stream(_: dict):
        nonlocal is_streaming
        is_streaming = False
        reset_detection_state()
        print("[CTRL] 분리배출 세션 종료 -> 대기 모드 (절전 전환)")

    command_dispatcher = {
        cfg.proto.CMD_START_STREAM: handle_start_stream,
        cfg.proto.CMD_STOP_STREAM: handle_stop_stream,
    }

    # 5. 오토 디텍트 처리 함수 (추후 도어 닫힘 상태 센서 연동 시 이 부분에 조건 추가)
    def process_auto_detection(detections: list[dict]):
        nonlocal current_target_class, consecutive_detected_count

        if not detections:
            reset_detection_state()
            return

        # 가장 신뢰도 높은 객체 선택
        best_det = max(detections, key=lambda x: x["confidence"])
        detected_name = best_det["class_name"]

        if detected_name == current_target_class:
            consecutive_detected_count += 1
        else:
            current_target_class = detected_name
            consecutive_detected_count = 1

        # 연속 검출 기준 충족 시 즉시 STM32로 해당 품목 투입 도어 개방 명령 전송
        if consecutive_detected_count >= cfg.ctrl.STABLE_FRAME_THRESHOLD:
            # TODO: 추후 도어 닫힘 상태 확인 조건(예: is_door_closed) 추가 예정 지점
            serial_ctrl.send_command(class_name=detected_name)

    print(f"\n[SERVER] 파이프라인 준비 완료 (TCP Port: {cfg.net.port})")
    print("[INFO] 터미널에서 'q' 키를 누르면 엔터 없이 즉시 종료됩니다.\n")

    prev_time = time.time()

    # 6. 메인 이벤트 루프
    try:
        while is_running:
            # 6-1. 종료 단축키 검사
            key = key_reader.get_key()
            if key and key.lower() == "q":
                print("\n[SYSTEM] 사용자 'q' 입력 감지 -> 정상 종료 시퀀스 시작")
                break

            # 6-2. 관제 PC 클라이언트 연결 수락
            if not socket_server.is_connected:
                is_streaming = False
                reset_detection_state()
                socket_server.accept_client()
                time.sleep(cfg.ctrl.ACCEPT_POLL_SEC)
                continue

            # 6-3. 관제 PC 세션 제어 명령 수신
            cmd_data = socket_server.receive_command()
            if cmd_data:
                cmd = cmd_data.get(cfg.proto.KEY_CMD)
                handler = command_dispatcher.get(cmd)
                if handler:
                    handler(cmd_data)
                else:
                    print(f"[CTRL WARN] 처리되지 않은 제어 명령: {cmd}")

            # 6-4. [절전 모드] 분리수거 화면이 아닐 때는 추론 및 전송 스킵
            if not is_streaming:
                time.sleep(cfg.ctrl.IDLE_SLEEP_SEC)
                continue

            # 6-5. 카메라 프레임 획득
            ret, frame = camera.read()
            if not ret or frame is None:
                continue

            # 6-6. TensorRT 추론
            t0 = time.time()
            detections = detector.detect(frame)
            infer_ms = (time.time() - t0) * 1000.0

            # 6-7. 자동 감지 및 시리얼 도어 개방 처리
            process_auto_detection(detections)

            # 6-8. FPS 계산
            curr_time = time.time()
            time_diff = curr_time - prev_time
            fps = 1.0 / time_diff if time_diff > 0 else 0.0
            prev_time = curr_time

            # 6-9. 관제 PC로 패킷 전송
            meta = {
                "timestamp": curr_time,
                "fps": round(fps, 1),
                "infer_ms": round(infer_ms, 2),
                "detections": detections,
            }
            socket_server.send_frame(frame, meta)

    finally:
        # 7. 리소스 안전 반환
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
