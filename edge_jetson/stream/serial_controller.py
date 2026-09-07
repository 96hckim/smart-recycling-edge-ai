"""
stream/serial_controller.py

STM32 UART I/O 통신 관리 모듈 (ProtocolParser 위임 구조)
"""

import threading
import time

try:
    import serial
    from serial import SerialException
except ImportError:
    serial = None  # type: ignore

    class SerialException(Exception):  # type: ignore
        pass


from stream.protocol import BinLevels, DoorAction, DoorState, DoorStatus, ProtocolParser


class SerialController:
    def __init__(
        self,
        port: str = "/dev/ttyTHS1",
        baudrate: int = 115200,
        timeout: float = 0.1,
        enabled: bool = False,
    ):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.enabled = enabled

        self.ser: serial.Serial | None = None
        self.running: bool = False
        self.rx_thread: threading.Thread | None = None

        self._lock = threading.Lock()
        self._bin_levels = BinLevels()
        self._door_status = DoorStatus()
        self._last_commanded_item: str = "ALL"

        if self.enabled:
            self._connect()
        else:
            print("[SERIAL] 시뮬레이션 모드로 시작합니다.")

    def _connect(self):
        if serial is None:
            print(
                "[SERIAL] pyserial 모듈이 설치되지 않아 시뮬레이션 모드로 동작합니다."
            )
            self.ser = None
            self.enabled = False
            return

        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
            self.running = True
            self.rx_thread = threading.Thread(
                target=self._rx_loop, name="SerialRxWorker", daemon=True
            )
            self.rx_thread.start()
            print(f"[SERIAL] 연결 성공 -> {self.port} ({self.baudrate} bps)")
        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] 포트 연결 실패: {e}")
            self.ser = None
            self.enabled = False

    def _rx_loop(self):
        """프로토콜 파서를 통해 수신 라인을 객체로 변환 및 상태 갱신"""
        while self.running and self.ser is not None:
            try:
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                packet_type, data = ProtocolParser.parse_mcu_line(line)
                if packet_type == "BIN" and isinstance(data, BinLevels):
                    with self._lock:
                        self._bin_levels = data
                elif packet_type == "DOOR" and isinstance(data, DoorStatus):
                    with self._lock:
                        # 하드웨어에서 보고한 상태에 최근 명령 품목명 매핑
                        reported_item = (
                            self._last_commanded_item
                            if data.state == DoorState.OPEN
                            else "ALL"
                        )
                        self._door_status = DoorStatus(
                            item=reported_item, state=data.state
                        )

            except (SerialException, OSError):
                time.sleep(0.01)

    def send_command(self, action: DoorAction, item_name: str | None = None) -> bool:
        """DoorAction 열거형과 품목명을 받아 프로토콜 규격으로 전송"""
        if action == DoorAction.OPEN:
            self._last_commanded_item = (item_name or "ALL").upper()
        else:
            self._last_commanded_item = "ALL"

        payload = ProtocolParser.encode_door_command(action, item_name)
        success = self._write(payload)

        # 시뮬레이션 모드에서는 송신 즉시 가상 도어 상태 갱신 (대시보드 동기화)
        if success and (not self.enabled or self.ser is None):
            with self._lock:
                state = (
                    DoorState.OPEN if action == DoorAction.OPEN else DoorState.CLOSED
                )
                self._door_status = DoorStatus(
                    item=self._last_commanded_item, state=state
                )

        return success

    def _write(self, text: str) -> bool:
        if not self.enabled or self.ser is None:
            print(f"[SERIAL SIMULATE TX] -> {text.strip()}")
            return True

        try:
            self.ser.write(text.encode("utf-8"))
            self.ser.flush()
            print(f"[SERIAL TX] -> {text.strip()}")
            return True
        except (SerialException, OSError) as e:
            print(f"[SERIAL ERROR] 송신 실패: {e}")
            return False

    def get_latest_data(self) -> tuple[dict[str, int], dict[str, str]]:
        """Qt 관제 전송용 딕셔너리 반환"""
        with self._lock:
            return self._bin_levels.to_dict(), self._door_status.to_dict()

    def close(self):
        self.running = False
        if self.rx_thread and self.rx_thread.is_alive():
            self.rx_thread.join(timeout=0.5)
            self.rx_thread = None

        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
                print(f"[SERIAL] {self.port} 연결 해제 완료")
            except (SerialException, OSError):
                pass
            finally:
                self.ser = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()
