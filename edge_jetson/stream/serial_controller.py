"""
STM32 UART 서보모터 제어 통신 모듈 (트리거 쿨다운 및 린터 경고 완전 해결)
"""

import time
from typing import Any

try:
    import serial
    from serial import SerialException
except ImportError:
    serial = None
    SerialException = OSError  # type: ignore[misc, assignment]


class SerialController:
    """
    Jetson -> STM32 간 UART 제어 명령(CAN, PET, PAPER 등)을 전송하고
    서보모터 동작 주기에 맞춘 중복 전송 방지(Cooldown)를 수행하는 컨트롤러
    """

    def __init__(
        self,
        port: str = "/dev/ttyTHS1",
        baudrate: int = 115200,
        timeout: float = 0.1,
        enabled: bool = False,
        cooldown_sec: float = 2.0,
    ):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.enabled = enabled
        self.cooldown_sec = cooldown_sec

        self.ser: Any | None = None
        self.last_trigger_time: float = 0.0
        self.last_triggered_class: str | None = None

        if self.enabled:
            self._connect()
        else:
            print("[SERIAL] STM32 시리얼 통신 비활성화 모드 (더미 로깅 전용)")

    def _connect(self):
        """시리얼 포트 연결 시도"""
        if serial is None:
            print(
                "[SERIAL ERROR] pyserial 모듈이 설치되지 않았습니다. pip install pyserial"
            )
            self.enabled = False
            return

        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
            print(f"[SERIAL] STM32 포트 연결 성공 -> {self.port} ({self.baudrate} bps)")
        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] STM32 포트({self.port}) 열기 실패: {e}")
            self.ser = None
            self.enabled = False

    def send_command(self, class_name: str, force: bool = False) -> bool:
        """
        STM32로 제어 명령 문자열 전송 (쿨다운 검사 포함)

        :param class_name: 감지된 클래스명 ("rock", "paper", "can", "pet" 등)
        :param force: 쿨다운을 무시하고 강제 전송할지 여부
        :return: 전송 성공 시 True, 쿨다운 중이거나 전송 실패 시 False
        """
        curr_time = time.time()

        # 1. 단일 if문으로 쿨다운 검사 (SIM102 린터 규칙 준수)
        if not force and (curr_time - self.last_trigger_time) < self.cooldown_sec:
            return False

        # 2. 프로토콜 패킷 생성 (대문자 + 개행문자)
        clean_name = class_name.strip().upper()
        payload = f"{clean_name}\n".encode()

        # 3. 비활성화 모드 시 콘솔 로그만 출력
        if not self.enabled or self.ser is None:
            print(f"[SERIAL SIMULATE] -> STM32 명령: {clean_name}")
            self.last_trigger_time = curr_time
            self.last_triggered_class = clean_name
            return True

        # 4. 실제 UART 전송
        try:
            self.ser.write(payload)
            self.ser.flush()
            self.last_trigger_time = curr_time
            self.last_triggered_class = clean_name
            print(f"[SERIAL TX] -> STM32: {clean_name}")
            return True

        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] 데이터 송신 중 오류: {e}")
            return False

    def reset_cooldown(self):
        """쿨다운 타이머 즉시 초기화 (필요 시 즉각 재트리거 가능)"""
        self.last_trigger_time = 0.0

    def close(self):
        """시리얼 포트 안전 해제"""
        if self.ser is not None:
            try:
                if hasattr(self.ser, "is_open") and self.ser.is_open:
                    self.ser.close()
                    print(f"[SERIAL] {self.port} 포트 해제 완료")
            except (SerialException, OSError, AttributeError):
                pass
            finally:
                self.ser = None

    def __del__(self):
        self.close()
