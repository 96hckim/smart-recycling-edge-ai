"""
configs/config.py

스마트 분리수거 시스템 전역 불변(Frozen) 설정 정의 모듈
"""

from dataclasses import dataclass, field
from pathlib import Path

JETSON_ROOT_DIR = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class ProtocolConfig:
    """Qt-Jetson 간 TCP 소켓 통신 JSON 프로토콜 규격 정의"""

    KEY_CMD: str = "cmd"
    CMD_START_STREAM: str = "START_STREAM"
    CMD_STOP_STREAM: str = "STOP_STREAM"


@dataclass(frozen=True)
class StreamControlConfig:
    """세션 감지 및 절전 루프 관련 파라미터"""

    STABLE_FRAME_THRESHOLD: int = 5  # 오인식 방지용 연속 프레임 임계값
    IDLE_SLEEP_SEC: float = 0.02  # 대기 모드 슬립 (약 50Hz)
    ACCEPT_POLL_SEC: float = 0.01  # 클라이언트 수락 폴링


@dataclass(frozen=True)
class CameraConfig:
    """카메라 입력 스트림 설정"""

    device_id: int = 0
    width: int = 640
    height: int = 480
    fps: int = 60
    buffer_size: int = 1
    flip_horizontal: bool = True


@dataclass(frozen=True)
class ModelConfig:
    """TensorRT 10 및 YOLOv11 검출기 설정"""

    engine_path: Path = JETSON_ROOT_DIR / "models" / "rps_yolo11n_custom_640.engine"
    input_shape: tuple[int, int] = (640, 640)
    conf_threshold: float = 0.50
    iou_threshold: float = 0.45
    class_names: tuple[str, ...] = ("paper", "rock", "scissors")


@dataclass(frozen=True)
class NetworkConfig:
    """PC 관제 대시보드 연동 TCP 소켓 설정"""

    host: str = "0.0.0.0"
    port: int = 9000
    jpeg_quality: int = 70
    socket_timeout: float = 1.0


@dataclass(frozen=True)
class SerialConfig:
    """STM32 UART 서보모터 제어 통신 설정"""

    port: str = "/dev/ttyTHS1"
    baudrate: int = 115200
    timeout: float = 0.1
    enabled: bool = False


@dataclass(frozen=True)
class AppConfig:
    """최상위 통합 설정 컨테이너"""

    cam: CameraConfig = field(default_factory=CameraConfig)
    model: ModelConfig = field(default_factory=ModelConfig)
    net: NetworkConfig = field(default_factory=NetworkConfig)
    serial: SerialConfig = field(default_factory=SerialConfig)
    proto: ProtocolConfig = field(default_factory=ProtocolConfig)
    ctrl: StreamControlConfig = field(default_factory=StreamControlConfig)


cfg = AppConfig()
