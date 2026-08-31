"""
시스템 전역 불변(Frozen) 설정 정의 모듈
"""

from dataclasses import dataclass, field
from pathlib import Path

# 프로젝트 루트 디렉터리 절대 경로 계산 (edge_jetson/)
JETSON_ROOT_DIR = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class CameraConfig:
    """카메라 입력 스트림 설정"""

    src: int | str = 0  # V4L2 인덱스(0) 또는 CSI GStreamer 문자열
    width: int = 640
    height: int = 480
    fps: int = 60
    buffer_size: int = 1  # 딜레이 방지를 위한 V4L2 큐 버퍼 최소화
    flip_horizontal: bool = True  # 좌우 반전(거울 모드) 활성화


@dataclass(frozen=True)
class ModelConfig:
    """TensorRT 및 YOLOv11 검출기 설정"""

    engine_path: Path = JETSON_ROOT_DIR / "models" / "rps_yolo11n_custom_640.engine"
    input_shape: tuple[int, int] = (640, 640)  # (Width, Height)
    conf_threshold: float = 0.50  # 신뢰도 임계값
    iou_threshold: float = 0.45  # NMS IOU 임계값

    # 현재: 임시 RPS -> 추후: ("CAN", "PET", "PAPER") 튜플로 교체
    class_names: tuple[str, ...] = ("paper", "rock", "scissors")


@dataclass(frozen=True)
class NetworkConfig:
    """PC 관제 대시보드 연동 TCP 소켓 스트리밍 설정"""

    host: str = "0.0.0.0"
    port: int = 8080
    jpeg_quality: int = 70  # 전송 프레임 압축률 (50~80 권장)
    socket_timeout: float = 1.0  # 소켓 수신/송신 타임아웃(초)


@dataclass(frozen=True)
class SerialConfig:
    """STM32 UART 서보모터 제어 통신 설정"""

    port: str = "/dev/ttyTHS1"  # Jetson 40Pin Header UART (또는 /dev/ttyUSB0)
    baudrate: int = 115200
    timeout: float = 0.1
    enabled: bool = False  # STM32 연결 여부 (데스크톱 단독 테스트 시 False)


@dataclass(frozen=True)
class AppConfig:
    """최상위 통합 설정 컨테이너"""

    cam: CameraConfig = field(default_factory=CameraConfig)
    model: ModelConfig = field(default_factory=ModelConfig)
    net: NetworkConfig = field(default_factory=NetworkConfig)
    serial: SerialConfig = field(default_factory=SerialConfig)


# 전역에서 import하여 사용할 불변 설정 싱글톤 객체
cfg = AppConfig()
