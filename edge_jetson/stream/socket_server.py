"""관제 PC 연동 초저지연 TCP 영상/메타데이터 스트리밍 서버 모듈."""

import json
import select
import socket
import struct
from typing import Any

import cv2
import numpy as np


class StreamSocketServer:
    """8바이트 바이너리 헤더 프로토콜 기반 JPEG 영상 및 JSON 텔레메트리 송신 서버."""

    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 9000,
        jpeg_quality: int = 70,
        timeout: float = 1.0,
    ):
        """서버 파라미터 초기화 및 리스닝 소켓 바인딩."""
        self.host = host
        self.port = port
        self.jpeg_quality = jpeg_quality
        self.timeout = timeout

        self._encode_params = (cv2.IMWRITE_JPEG_QUALITY, self.jpeg_quality)

        self.server_socket: socket.socket | None = None
        self.client_socket: socket.socket | None = None
        self.client_addr: tuple | None = None

        self._init_server_socket()

    def _init_server_socket(self):
        """TIME_WAIT 포트 재바인딩(SO_REUSEADDR) 설정 및 수신 대기 소켓 생성."""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.server_socket.settimeout(self.timeout)
        print(f"[NET] TCP 서버 바인딩 완료 -> {self.host}:{self.port}")

    @property
    def is_connected(self) -> bool:
        """클라이언트 소켓 연결 유지 여부 반환."""
        return self.client_socket is not None

    def accept_client(self) -> bool:
        """select() 기반 논블로킹(0초) 클라이언트 접속 수락 및 저지연 소켓 옵션 적용."""
        if self.is_connected or self.server_socket is None:
            return False

        try:
            readable, _, _ = select.select([self.server_socket], [], [], 0)
            if not readable:
                return False

            client, addr = self.server_socket.accept()

            # Nagle 알고리즘 비활성화(초저지연) 및 대용량 송신 버퍼(256KB) 설정
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
        """헤더 8B(이미지 길이 4B + JSON 길이 4B, Big-Endian) 패킹 및 일괄 sendall 송신."""
        if not self.is_connected or self.client_socket is None:
            return False

        try:
            success, encimg = cv2.imencode(".jpg", frame, self._encode_params)
            if not success:
                return False
            img_bytes = encimg.tobytes()

            json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")

            # Big-Endian uint32: [이미지 바이트 수(4B)] + [JSON 바이트 수(4B)]
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

    def close_client(self):
        """클라이언트 소켓 shutdown/close 및 FD 누수 방지."""
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
        """서버 소켓 및 클라이언트 연결 안전 해제."""
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
