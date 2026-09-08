"""스마트 재활용 키오스크 전역 불변(Frozen) 설정 모듈."""

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path

JETSON_ROOT_DIR = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class CameraConfig:
    """V4L2 카메라 캡처 파라미터 설정."""

    device_id: int = 0
    width: int = 640
    height: int = 480
    fps: int = 60
    buffer_size: int = 1  # 큐 프레임 지연(Lag) 방지용 최소 버퍼 크기
    flip_horizontal: bool = True  # 키오스크 인터랙션용 좌우 반전


class Category(str, Enum):
    """재활용 대상 4종 품목 열거형."""

    PET = "PET"
    CAN = "CAN"
    PAPER = "PAPER"
    VINYL = "VINYL"


@dataclass(frozen=True)
class ModelClassMeta:
    """YOLO 모델 출력 인덱스와 도메인 품목 정보 간의 1:1 매핑 메타데이터."""

    class_id: int
    name_en: str
    name_ko: str
    category: Category


# YOLO 모델 학습 순서 기준 인덱스 1:1 매핑 테이블 (0: 페트, 1: 캔, 2: 종이, 3: 비닐)
MODEL_CLASS_MAP: tuple[ModelClassMeta, ...] = (
    ModelClassMeta(0, "pet", "페트", Category.PET),
    ModelClassMeta(1, "can", "캔", Category.CAN),
    ModelClassMeta(2, "paper", "종이", Category.PAPER),
    ModelClassMeta(3, "vinyl", "비닐", Category.VINYL),
)


@dataclass(frozen=True)
class ModelConfig:
    """YOLOv11 TensorRT 엔진 경로 및 추론 임계값 설정."""

    engine_path: Path = JETSON_ROOT_DIR / "models" / "recycle_yolo11n_640.engine"
    input_shape: tuple[int, int] = (640, 640)
    conf_threshold: float = 0.50
    iou_threshold: float = 0.45
    # YOLO 모델 학습 클래스 순서 (0: 페트, 1: 캔, 2: 종이, 3: 비닐)
    class_names: tuple[str, ...] = tuple(meta.name_en for meta in MODEL_CLASS_MAP)


@dataclass(frozen=True)
class NetworkConfig:
    """관제 PC 연동 TCP 영상 스트리밍 소켓 설정."""

    host: str = "0.0.0.0"
    port: int = 9000
    jpeg_quality: int = 70  # 전송 대역폭 절감과 화질 간 최적 균형값
    socket_timeout: float = 1.0


@dataclass(frozen=True)
class SerialConfig:
    """STM32 MCU UART 시리얼 통신 설정."""

    port: str = "/tmp/ttyV0"  # "/dev/ttyTHS1"
    baudrate: int = 115200
    timeout: float = 0.1
    enabled: bool = True


@dataclass(frozen=True)
class DoorConfig:
    """수거함 도어 FSM 디바운스 및 타임아웃 파라미터."""

    stable_frames: int = 15  # 오검출 방지용 연속 인식 프레임 수 (약 0.5초)
    min_hold_sec: float = 2.0  # 투입 안전을 위한 최소 개방 유지 시간 (초)
    lost_tolerance: int = 15  # 깜빡임/가림 허용 부재 프레임 수 (약 0.5초)
    max_open_sec: float = 10.0  # 모터 보호 및 방치 방지용 최대 개방 제한 시간 (초)


@dataclass(frozen=True)
class AppConfig:
    """전체 서브시스템 통합 설정 컨테이너."""

    cam: CameraConfig = field(default_factory=CameraConfig)
    model: ModelConfig = field(default_factory=ModelConfig)
    net: NetworkConfig = field(default_factory=NetworkConfig)
    serial: SerialConfig = field(default_factory=SerialConfig)
    door: DoorConfig = field(default_factory=DoorConfig)


# 전역 설정 싱글톤 인스턴스
cfg = AppConfig()
