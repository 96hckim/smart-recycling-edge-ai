"""관제 PC 연동 초저지연 TCP 영상/메타데이터 스트리밍 서버 모듈."""

import json
import select
import socket
import struct
from typing import Any

import cv2
import numpy as np


class StreamSocketServer:
    """8바이트 바이너리 헤더 프로토콜 기반 실시간 영상/텔레메트리 송신 서버."""

    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 9000,
        jpeg_quality: int = 70,
        timeout: float = 1.0,
    ):
        """서버 소켓 생성 및 압축 파라미터 초기화."""
        self.host = host
        self.port = port
        self.jpeg_quality = jpeg_quality
        self.timeout = timeout

        self._encode_params = (cv2.IMWRITE_JPEG_QUALITY, self.jpeg_quality)

        self.server_socket: socket.socket | None = None
        self.client_socket: socket.socket | None = None
        self.client_addr: tuple | None = None
        self._rx_buffer: bytearray = bytearray()

        self._init_server_socket()

    def _init_server_socket(self):
        """TIME_WAIT 상태 포트 즉시 재사용(SO_REUSEADDR) 설정 및 바인딩."""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.server_socket.settimeout(self.timeout)
        print(f"[NET] TCP 서버 바인딩 완료 -> {self.host}:{self.port}")

    @property
    def is_connected(self) -> bool:
        """클라이언트 유효 연결 상태 확인."""
        return self.client_socket is not None

    def accept_client(self) -> bool:
        """select() 기반 비차단 접속 수락 및 네트워크 저지연 옵션 구성."""
        if self.is_connected or self.server_socket is None:
            return False

        try:
            readable, _, _ = select.select([self.server_socket], [], [], 0)
            if not readable:
                return False

            client, addr = self.server_socket.accept()

            # 실시간 영상 지연 최소화를 위한 Nagle 알고리즘 해제 및 대용량 송신 버퍼 확보
            client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            client.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 256 * 1024)
            client.settimeout(self.timeout)

            self.client_socket = client
            self.client_addr = addr
            print(f"[NET] 관제 PC 연결 수락: {addr}")
            return True

        except (TimeoutError, BlockingIOError):
            return False
        except OSError as e:
            print(f"[NET ERROR] 클라이언트 연결 실패: {e}")
            return False

    def send_frame(self, frame: np.ndarray, metadata: dict[str, Any]) -> bool:
        """JPEG 인코딩 후 8B 헤더(이미지 길이 4B + 메타데이터 길이 4B)와 함께 전송."""
        if not self.is_connected or self.client_socket is None:
            return False

        try:
            success, encimg = cv2.imencode(".jpg", frame, self._encode_params)
            if not success:
                return False
            img_bytes = encimg.tobytes()

            json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")

            # 패킷 구조: [Image Size(uint32, 4B)] + [JSON Size(uint32, 4B)] (Big-Endian)
            header = struct.pack(">II", len(img_bytes), len(json_bytes))

            self.client_socket.sendall(header + img_bytes + json_bytes)
            return True

        except (TimeoutError, BrokenPipeError, ConnectionResetError):
            print(f"[NET] 관제 PC({self.client_addr}) 연결 끊김 감지")
            self.close_client()
            return False
        except OSError as e:
            print(f"[NET ERROR] 데이터 전송 오류: {e}")
            self.close_client()
            return False

    def receive_commands(self) -> list[dict[str, Any]]:
        """관제 PC(Qt)로부터 전송된 JSON 개별 명령(\\n 구분자)을 비차단(Non-blocking)으로 수신."""
        if not self.is_connected or self.client_socket is None:
            return []

        commands: list[dict[str, Any]] = []

        try:
            # 0초 타임아웃 select로 수신 버퍼 가용 여부 즉각 확인 (0ms 대기, 메인 루프 블로킹 없음)
            readable, _, _ = select.select([self.client_socket], [], [], 0)
            if readable:
                chunk = self.client_socket.recv(4096)
                if not chunk:
                    # 빈 바이트 수신은 상대방의 정상 소켓 close()를 의미
                    print(f"[NET] 관제 PC({self.client_addr}) 정상 연결 종료 감지")
                    self.close_client()
                    return []
                self._rx_buffer.extend(chunk)

            # 수신 버퍼에서 개행 문자(\n) 단위로 완전한 JSON 패킷 추출
            while b"\n" in self._rx_buffer:
                line, rest = self._rx_buffer.split(b"\n", 1)
                self._rx_buffer = bytearray(rest)
                clean_line = line.strip()
                if not clean_line:
                    continue
                try:
                    cmd_dict = json.loads(clean_line.decode("utf-8", errors="ignore"))
                    if isinstance(cmd_dict, dict):
                        commands.append(cmd_dict)
                except json.JSONDecodeError as err:
                    print(
                        f"[NET WARN] 수신 JSON 파싱 오류: {err} -> 원문: {clean_line[:50]}"
                    )

        except (TimeoutError, BrokenPipeError, ConnectionResetError):
            print(f"[NET] 관제 PC({self.client_addr}) 소켓 연결 끊김 감지")
            self.close_client()
            return []
        except OSError as e:
            print(f"[NET ERROR] 제어 명령 수신 오류: {e}")
            self.close_client()
            return []

        return commands

    def close_client(self):
        """클라이언트 소켓의 양방향 셧다운 및 파일 디스크립터 누수 방지."""
        self._rx_buffer = bytearray()
        if self.client_socket is not None:
            try:
                self.client_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass

            try:
                self.client_socket.close()
            except OSError:
                pass
            finally:
                self.client_socket = None
                self.client_addr = None

    def close(self):
        """서버 리스닝 소켓 및 활성 클라이언트 연결 완전 정리."""
        if self.server_socket is None and self.client_socket is None:
            return

        self.close_client()

        if self.server_socket is not None:
            try:
                self.server_socket.close()
            except OSError:
                pass
            finally:
                self.server_socket = None
            print(f"[NET] 포트 {self.port} 소켓 정상 반환")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()
