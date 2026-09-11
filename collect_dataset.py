"""스마트 재활용 키오스크 YOLOv11 데이터셋 수동 캡처 파이프라인.

Target Hardware: Logitech C270 HD Webcam (1280x720 @ 30fps, USB 2.0 UVC)
Target Model: YOLOv11 (Paper, Can, Pet, Vinyl 4-class + Background Negative)
Controls:
    - [S]        : 현재 프레임 캡처 (비동기 초고속 디스크 저장)
    - [1 ~ 4]    : 수집 품목 선택 (1: Paper, 2: Can, 3: Pet, 4: Vinyl)
    - [0 / 5 / B]: 빈 배경(Background) 선택 (0바이트 빈 .txt 자동 생성)
    - [G]        : YOLO 1:1 정방형 ROI 가이드라인 토글
    - [Q / ESC]  : 안전 종료 및 디스크 플러시
"""

import os
import platform
import queue
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import cv2
import numpy as np


# ==============================================================================
# 1. 전역 불변 설정 컨테이너 (Configuration)
# ==============================================================================
@dataclass(frozen=True)
class CollectorConfig:
    """데이터 수집기 파라미터 불변 설정."""

    # 카메라 하드웨어 파라미터 (Logitech C270 네이티브 규격)
    camera_id: int = 0
    width: int = 1280
    height: int = 720
    fps: int = 30
    fourcc: str = "MJPG"  # C270 USB 2.0 대역폭 병목 방지 필수 코덱
    buffer_size: int = 1  # 드라이버 내부 버퍼 지연(Latency) 방지

    # 데이터셋 저장 파라미터
    save_base_dir: str = "dataset/raw_images"
    jpeg_quality: int = 95  # YOLO 학습용 무손실 수준 JPEG 압축률

    # 빈 배경 클래스 식별자
    background_class_name: str = "background"

    # 클래스 매핑 (1: paper, 2: can, 3: pet, 4: vinyl, 0/5/B: background)
    classes: dict[int, str] = field(
        default_factory=lambda: {
            ord("1"): "paper",
            ord("2"): "can",
            ord("3"): "pet",
            ord("4"): "vinyl",
            ord("0"): "background",
            ord("5"): "background",
            ord("b"): "background",
            ord("B"): "background",
        }
    )


# ==============================================================================
# 2. 비동기 디스크 I/O 워커 (Async Disk Writer)
# ==============================================================================
class AsyncImageWriter:
    """메인 영상 루프의 프리징을 방지하기 위한 백그라운드 큐 기반 비동기 이미지 저장기."""

    def __init__(self, queue_size: int = 256, jpeg_quality: int = 95) -> None:
        self._queue: queue.Queue[tuple[str, np.ndarray] | None] = queue.Queue(
            maxsize=queue_size
        )
        self._jpeg_params = [int(cv2.IMWRITE_JPEG_QUALITY), jpeg_quality]
        self._running = True
        self._dropped_count = 0
        self._saved_count = 0

        self._worker_thread = threading.Thread(
            target=self._worker_loop, name="AsyncWriterWorker", daemon=True
        )
        self._worker_thread.start()

    def _worker_loop(self) -> None:
        while self._running or not self._queue.empty():
            try:
                task = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue

            if task is None:
                self._queue.task_done()
                break

            filepath, img = task
            try:
                cv2.imwrite(filepath, img, self._jpeg_params)
                self._saved_count += 1
            except Exception as exc:
                print(
                    f"\n[ERROR] 이미지 저장 실패 ({filepath}): {exc}",
                    file=sys.stderr,
                )
            finally:
                self._queue.task_done()

    def submit(self, filepath: str, img: np.ndarray) -> bool:
        """프레임을 큐에 등록 (메인 루프 블로킹 0ms 보장)."""
        try:
            # 원본 프레임 버퍼 변경을 방지하기 위해 복제본 전달
            self._queue.put_nowait((filepath, img.copy()))
            return True
        except queue.Full:
            self._dropped_count += 1
            print(
                f"\n[WARNING] 디스크 쓰기 큐 포화: 프레임 드랍 발생 (총 {self._dropped_count}건)",
                file=sys.stderr,
            )
            return False

    def close(self) -> None:
        """대기 중인 모든 큐 프레임을 디스크에 플러시 후 종료."""
        self._running = False
        try:
            self._queue.put_nowait(None)
        except queue.Full:
            pass

        self._queue.join()
        if self._worker_thread.is_alive():
            self._worker_thread.join(timeout=2.0)

    @property
    def pending_count(self) -> int:
        return self._queue.qsize()

    @property
    def total_saved(self) -> int:
        return self._saved_count


# ==============================================================================
# 3. 로지텍 C270 하드웨어 최적화 팩토리
# ==============================================================================
def create_optimized_camera(
    cfg: CollectorConfig,
) -> tuple[cv2.VideoCapture, dict[str, Any]]:
    """로지텍 C270 웹캠 특성에 맞춰 UVC 파라미터를 명시적으로 바인딩."""
    is_windows = platform.system() == "Windows"
    backend = cv2.CAP_DSHOW if is_windows else cv2.CAP_V4L2

    cap = cv2.VideoCapture(cfg.camera_id, backend)
    if not cap.isOpened() and is_windows:
        cap = cv2.VideoCapture(cfg.camera_id)

    if not cap.isOpened():
        raise RuntimeError(
            f"카메라 디바이스(ID: {cfg.camera_id})를 열 수 없습니다. 연결 상태를 확인하세요."
        )

    # 1. USB 2.0 대역폭 보장을 위한 MJPG 압축 코덱 강제 적용
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*cfg.fourcc))

    # 2. 네이티브 HD 해상도 및 FPS 설정
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, cfg.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, cfg.height)
    cap.set(cv2.CAP_PROP_FPS, cfg.fps)

    # 3. 하드웨어 드라이버 큐 지연(Latency) 방지
    cap.set(cv2.CAP_PROP_BUFFERSIZE, cfg.buffer_size)

    actual_fourcc = int(cap.get(cv2.CAP_PROP_FOURCC))
    fourcc_str = "".join([chr((actual_fourcc >> 8 * i) & 0xFF) for i in range(4)])

    actual_params: dict[str, Any] = {
        "width": int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)),
        "height": int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)),
        "fps": float(cap.get(cv2.CAP_PROP_FPS)),
        "fourcc_str": fourcc_str,
        "backend": str(cap.getBackendName()),
    }

    return cap, actual_params


# ==============================================================================
# 4. GUI 및 가이드라인 렌더러 (HUD Renderer)
# ==============================================================================
class OverlayRenderer:
    """작업자 편의 및 실시간 수집 카운트 피드백을 위한 HUD 오버레이어."""

    COLOR_RED = (0, 0, 255)
    COLOR_GREEN = (0, 255, 0)
    COLOR_CYAN = (255, 255, 0)
    COLOR_YELLOW = (0, 255, 255)
    COLOR_WHITE = (255, 255, 255)
    COLOR_DARK_GRAY = (35, 35, 35)

    @classmethod
    def draw_roi_guide(
        cls, frame: np.ndarray, show: bool = True
    ) -> tuple[int, int, int, int] | None:
        """YOLO 640x640 정방형 훈련 영역을 고려한 중앙 1:1 ROI 가이드라인."""
        if not show:
            return None

        h, w = frame.shape[:2]
        box_size = min(h, w)
        x1 = (w - box_size) // 2
        y1 = (h - box_size) // 2
        x2 = x1 + box_size
        y2 = y1 + box_size

        cv2.rectangle(frame, (x1, y1), (x2, y2), cls.COLOR_YELLOW, 1, cv2.LINE_AA)
        cv2.putText(
            frame,
            "YOLO 1:1 Target ROI",
            (x1 + 10, y1 + 25),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            cls.COLOR_YELLOW,
            1,
            cv2.LINE_AA,
        )
        return (x1, y1, x2, y2)

    @classmethod
    def draw_dashboard(
        cls,
        frame: np.ndarray,
        current_class: str,
        class_counts: dict[str, int],
        fps: float,
        pending_io: int,
        feedback_timer: float,
    ) -> None:
        """실시간 품목별 수집 현황 및 캡처 피드백 HUD 렌더링."""
        h, _ = frame.shape[:2]

        # 1. 상단 대시보드 배경 반투명 패널
        panel_w = 580
        panel_h = 115
        cv2.rectangle(
            frame, (10, 10), (10 + panel_w, 10 + panel_h), cls.COLOR_DARK_GRAY, -1
        )
        border_color = cls.COLOR_GREEN if feedback_timer > 0 else cls.COLOR_WHITE
        cv2.rectangle(frame, (10, 10), (10 + panel_w, 10 + panel_h), border_color, 1)

        # 2. 시스템 상태 (FPS 및 디스크 큐)
        stat_text = f"FPS: {fps:4.1f} | I/O Queue: {pending_io:02d}"
        cv2.putText(
            frame,
            stat_text,
            (25, 34),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.50,
            cls.COLOR_CYAN,
            1,
            cv2.LINE_AA,
        )

        # 3. 현재 선택 품목 및 해당 품목 누적 수량 강조
        cur_count = class_counts.get(current_class, 0)
        target_text = f"Target: [{current_class.upper()}] -> {cur_count} imgs"
        cv2.putText(
            frame,
            target_text,
            (25, 66),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.75,
            cls.COLOR_GREEN,
            2,
            cv2.LINE_AA,
        )

        # 4. 전체 품목 수집 현황 통계 (4종 재활용 + 빈 배경)
        all_counts_text = (
            f"Paper: {class_counts.get('paper', 0)} | "
            f"Can: {class_counts.get('can', 0)} | "
            f"Pet: {class_counts.get('pet', 0)} | "
            f"Vinyl: {class_counts.get('vinyl', 0)} | "
            f"BG: {class_counts.get('background', 0)}"
        )
        cv2.putText(
            frame,
            all_counts_text,
            (25, 96),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.45,
            cls.COLOR_WHITE,
            1,
            cv2.LINE_AA,
        )

        # 5. 수동 캡처 시각적 플래시 피드백 (화면 모서리 녹색 테두리 및 배너)
        if feedback_timer > 0:
            cv2.rectangle(
                frame, (0, 0), (frame.shape[1] - 1, h - 1), cls.COLOR_GREEN, 3
            )
            feedback_text = f"CAPTURED! ({current_class.upper()} #{cur_count})"
            cv2.putText(
                frame,
                feedback_text,
                (frame.shape[1] - 340, 45),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.65,
                cls.COLOR_GREEN,
                2,
                cv2.LINE_AA,
            )

        # 6. 하단 단축키 조작 가이드
        guide_text = (
            "[S]: Capture | [1~4]: Class | [0/5/B]: BG | [G]: ROI Guide | [Q/ESC]: Quit"
        )
        cv2.putText(
            frame,
            guide_text,
            (15, h - 15),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.50,
            cls.COLOR_WHITE,
            1,
            cv2.LINE_AA,
        )


# ==============================================================================
# 5. 디스크 기존 데이터셋 카운트 유틸리티
# ==============================================================================
def count_existing_images(base_dir: str, class_names: list[str]) -> dict[str, int]:
    """저장 디렉터리를 스캔하여 각 품목별 기존 누적 이미지 파일 수를 집계."""
    counts: dict[str, int] = {}
    valid_exts = (".jpg", ".jpeg", ".png")

    for cname in class_names:
        dir_path = os.path.join(base_dir, cname)
        if not os.path.isdir(dir_path):
            counts[cname] = 0
            continue

        try:
            cnt = sum(1 for f in os.listdir(dir_path) if f.lower().endswith(valid_exts))
            counts[cname] = cnt
        except OSError:
            counts[cname] = 0

    return counts


# ==============================================================================
# 6. 메인 컨트롤러 루프 (Main Pipeline Loop)
# ==============================================================================
def main() -> None:
    cfg = CollectorConfig()
    class_list = ["paper", "can", "pet", "vinyl", cfg.background_class_name]

    print("\n" + "=" * 70)
    print(" [Edge AI] 스마트 재활용 키오스크 수동 데이터셋 수집기 (C270 HD)")
    print("=" * 70)

    # 1. 카메라 하드웨어 초기화
    try:
        cap, actual_params = create_optimized_camera(cfg)
        print(f"[*] 카메라 연결 성공 (Backend: {actual_params['backend']})")
        print(f"    - 해상도: {actual_params['width']}x{actual_params['height']}")
        print(
            f"    - 코덱  : {actual_params['fourcc_str']} (FPS: {actual_params['fps']:.1f})"
        )
    except Exception as exc:
        print(f"[FATAL] 카메라 초기화 실패: {exc}", file=sys.stderr)
        return

    # 2. 비동기 디스크 라이터 및 기존 데이터셋 수량 스캔
    writer = AsyncImageWriter(queue_size=256, jpeg_quality=cfg.jpeg_quality)
    class_counts = count_existing_images(cfg.save_base_dir, class_list)

    current_class = "pet"
    show_roi_guide = True
    capture_feedback_duration = 0.25  # 캡처 시각 피드백 유지 시간 (초)
    feedback_until = 0.0

    # FPS 계측
    prev_tick = time.perf_counter()
    measured_fps = 0.0
    fps_smoothing = 0.9

    window_name = "Recycle Dataset Collector - Manual Mode"
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(window_name, cfg.width, cfg.height)

    print("\n[작업 준비 완료]")
    print(" - S Key      : 현재 프레임 즉시 캡처 (Clean Frame 비동기 저장)")
    print(" - 1~4 Key    : 품목 선택 (1:paper, 2:can, 3:pet, 4:vinyl)")
    print(" - 0/5/B Key  : 빈 배경(background) 모드 선택 (빈 라벨 동시 생성)")
    print(" - G Key      : YOLO 1:1 ROI 가이드라인 표시 토글")
    print(" - Q / ESC    : 안전 종료 및 디스크 플러시")
    print(
        f"\n[현재 저장소 집계] -> "
        f"Paper: {class_counts.get('paper', 0)} | Can: {class_counts.get('can', 0)} | "
        f"Pet: {class_counts.get('pet', 0)} | Vinyl: {class_counts.get('vinyl', 0)} | "
        f"BG: {class_counts.get('background', 0)}\n"
    )

    try:
        while True:
            ret, frame = cap.read()
            if not ret or frame is None:
                print(
                    "\n[WARNING] 카메라 프레임을 읽을 수 없습니다.",
                    file=sys.stderr,
                )
                time.sleep(0.01)
                continue

            now = time.perf_counter()
            dt = now - prev_tick
            prev_tick = now
            if dt > 0:
                current_fps = 1.0 / dt
                measured_fps = (
                    fps_smoothing * measured_fps + (1.0 - fps_smoothing) * current_fps
                )

            # ----------------------------------------------------------
            # 1. 키보드 이벤트 디스패치 (수동 캡처 및 제어)
            # ----------------------------------------------------------
            key = cv2.waitKey(1) & 0xFF

            if key in (ord("q"), 27):  # Q or ESC
                print("\n[*] 프로그램 종료 요청을 수신했습니다.")
                break

            # ROI 가이드라인 토글
            if key in (ord("g"), ord("G")):
                show_roi_guide = not show_roi_guide

            # 품목 클래스 전환 (1~4: 재활용품, 0/5/B: 빈 배경)
            if key in cfg.classes:
                current_class = cfg.classes[key]
                print(
                    f"[*] 타깃 품목 전환 -> [{current_class.upper()}] "
                    f"(현재 수집량: {class_counts[current_class]}장)"
                )

            # 수동 캡처 (S 키)
            if key in (ord("s"), ord("S")):
                target_dir = os.path.join(cfg.save_base_dir, current_class)
                os.makedirs(target_dir, exist_ok=True)

                timestamp_str = time.strftime("%Y%m%d_%H%M%S")
                # 밀리초 3자리 정밀 타임스탬프 추가로 키 연타 시 파일명 중복 방지
                ms = int((time.time() % 1) * 1000)
                next_index = class_counts[current_class] + 1
                stem = f"{current_class}_{timestamp_str}_{ms:03d}_{next_index:04d}"
                filename = f"{stem}.jpg"
                filepath = os.path.join(target_dir, filename)

                # UI 텍스트가 없는 순수 원본 프레임 비동기 저장 큐 제출
                if writer.submit(filepath, frame):
                    class_counts[current_class] += 1
                    feedback_until = now + capture_feedback_duration

                    # 빈 배경(Negative Sample) 모드일 경우: 0바이트 빈 .txt 라벨 자동 생성
                    txt_info_str = ""
                    if current_class == cfg.background_class_name:
                        target_txt_dir = os.path.join(
                            cfg.save_base_dir, f"{current_class}_txt"
                        )
                        os.makedirs(target_txt_dir, exist_ok=True)
                        txt_filename = f"{stem}.txt"
                        txt_filepath = os.path.join(target_txt_dir, txt_filename)
                        try:
                            Path(txt_filepath).touch(exist_ok=True)
                            txt_info_str = f" (+빈 라벨: {txt_filename})"
                        except OSError as exc:
                            print(
                                f"\n[ERROR] 빈 라벨 생성 실패 ({txt_filepath}): {exc}",
                                file=sys.stderr,
                            )

                    print(
                        f"[CAPTURE] [{current_class.upper()}] #{class_counts[current_class]} "
                        f"저장 -> {filename}{txt_info_str}"
                    )

            # ----------------------------------------------------------
            # 2. GUI 렌더링 (화면 프리뷰 전용 UI 합성)
            # ----------------------------------------------------------
            display_frame = frame.copy()
            OverlayRenderer.draw_roi_guide(display_frame, show_roi_guide)

            remaining_feedback = max(0.0, feedback_until - now)
            OverlayRenderer.draw_dashboard(
                frame=display_frame,
                current_class=current_class,
                class_counts=class_counts,
                fps=measured_fps,
                pending_io=writer.pending_count,
                feedback_timer=remaining_feedback,
            )

            cv2.imshow(window_name, display_frame)

    except KeyboardInterrupt:
        print("\n[*] KeyboardInterrupt 감지. 안전하게 종료합니다.")

    finally:
        # ----------------------------------------------------------
        # 3. 리소스 안전 해제 및 디스크 I/O 플러시
        # ----------------------------------------------------------
        print("[*] 카메라 장치 해제 중...")
        cap.release()
        cv2.destroyAllWindows()

        if writer.pending_count > 0:
            print(
                f"[*] 남은 디스크 쓰기 작업 대기 중 ({writer.pending_count}개 남음)..."
            )
        writer.close()

        print("\n[수집 완료 최종 통계]")
        for cname in class_list:
            extra_info = ""
            if cname == cfg.background_class_name:
                txt_dir = os.path.join(cfg.save_base_dir, f"{cname}_txt")
                txt_count = (
                    sum(1 for f in os.listdir(txt_dir) if f.lower().endswith(".txt"))
                    if os.path.isdir(txt_dir)
                    else 0
                )
                extra_info = f" (빈 라벨: {txt_count}개)"
            print(f" - {cname.upper():<11}: {class_counts.get(cname, 0)}장{extra_info}")
        print(f" - 이번 세션 총 이미지 기록: {writer.total_saved}장")
        print("=" * 70 + "\n")


if __name__ == "__main__":
    main()
