"""
stream/serial_controller.py

STM32 UART 서보모터 제어 통신 모듈
"""

import time

import serial
from serial import SerialException


class SerialController:
    """STM32로 쓰레기 분류 명령을 전송하고 모터 동작 쿨다운을 관리하는 컨트롤러"""

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

        self.ser: serial.Serial | None = None
        self.last_trigger_time: float = 0.0
        self.last_triggered_class: str | None = None

        if self.enabled:
            self._connect()
        else:
            print("[SERIAL] STM32 시리얼 비활성화 모드 (콘솔 시뮬레이션 동작)")

    def _connect(self):
        """시리얼 포트 연결 초기화"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
            print(f"[SERIAL] STM32 연결 완료 -> {self.port} ({self.baudrate} bps)")
        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] STM32 포트({self.port}) 열기 실패: {e}")
            self.ser = None
            self.enabled = False

    def send_command(self, class_name: str, force: bool = False) -> bool:
        """
        STM32로 분류 명령 전송 (대문자 + 개행문자)

        :param class_name: 분류 클래스명 ("can", "pet", "paper" 등)
        :param force: 쿨다운을 무시하고 즉시 강제 전송 여부
        """
        curr_time = time.time()

        # 모터 동작 중 중복 전송 방지 (쿨다운 검사)
        if not force and (curr_time - self.last_trigger_time) < self.cooldown_sec:
            return False

        clean_name = class_name.strip().upper()
        payload = f"{clean_name}\n".encode()

        # 시뮬레이션 모드 (보드 미연결 시 로그 출력)
        if not self.enabled or self.ser is None:
            print(f"[SERIAL SIMULATE] -> STM32 명령: {clean_name}")
            self.last_trigger_time = curr_time
            self.last_triggered_class = clean_name
            return True

        # 실제 UART 데이터 송신
        try:
            self.ser.write(payload)
            self.ser.flush()
            self.last_trigger_time = curr_time
            self.last_triggered_class = clean_name
            print(f"[SERIAL TX] -> STM32: {clean_name}")
            return True
        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] 데이터 송신 실패: {e}")
            return False

    def reset_cooldown(self):
        """쿨다운 타이머 강제 초기화"""
        self.last_trigger_time = 0.0

    def close(self):
        """시리얼 포트 안전 해제 (중복 호출 안전)"""
        if self.ser is not None:
            try:
                if self.ser.is_open:
                    self.ser.close()
                    print(f"[SERIAL] {self.port} 포트 해제 완료")
            except (SerialException, OSError) as e:
                print(f"[SERIAL ERROR] 포트 해제 예외: {e}")
            finally:
                self.ser = None

    def __del__(self):
        self.close()
