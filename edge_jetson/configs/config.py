"""
configs/config.py

스마트 분리수거 시스템 전역 불변(Frozen) 설정 정의 모듈
"""

from dataclasses import dataclass, field
from pathlib import Path

# 프로젝트 루트 디렉터리 경로 (edge_jetson/)
JETSON_ROOT_DIR = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class CameraConfig:
    """카메라 입력 스트림 설정"""

    device_id: int = 0  # V4L2 카메라 장치 번호 (/dev/video0)
    width: int = 640
    height: int = 480
    fps: int = 60
    buffer_size: int = 1  # 딜레이 방지용 V4L2 큐 버퍼 크기
    flip_horizontal: bool = True  # 좌우 반전(거울 모드) 활성화


@dataclass(frozen=True)
class ModelConfig:
    """TensorRT 10 및 YOLOv11 검출기 설정"""

    engine_path: Path = JETSON_ROOT_DIR / "models" / "rps_yolo11n_custom_640.engine"
    input_shape: tuple[int, int] = (640, 640)
    conf_threshold: float = 0.50  # 신뢰도 임계값
    iou_threshold: float = 0.45  # NMS IOU 임계값
    class_names: tuple[str, ...] = (
        "paper",
        "rock",
        "scissors",
    )  # 추후: ("CAN", "PET", "PAPER")


@dataclass(frozen=True)
class NetworkConfig:
    """PC 관제 대시보드 연동 TCP 소켓 설정"""

    host: str = "0.0.0.0"
    port: int = 9000  # 충돌 방지용 커스텀 포트 (기존 8080 대체)
    jpeg_quality: int = 70  # 전송 이미지 압축률 (1~100)
    socket_timeout: float = 1.0  # 소켓 입출력 타임아웃(초)


@dataclass(frozen=True)
class SerialConfig:
    """STM32 UART 서보모터 제어 통신 설정"""

    port: str = "/dev/ttyTHS1"  # Jetson 40Pin UART
    baudrate: int = 115200
    timeout: float = 0.1
    enabled: bool = False  # 하드웨어 보드 연결 시 True로 전환


@dataclass(frozen=True)
class AppConfig:
    """최상위 통합 설정 컨테이너"""

    cam: CameraConfig = field(default_factory=CameraConfig)
    model: ModelConfig = field(default_factory=ModelConfig)
    net: NetworkConfig = field(default_factory=NetworkConfig)
    serial: SerialConfig = field(default_factory=SerialConfig)


# 전역 설정 싱글톤 인스턴스
cfg = AppConfig()
