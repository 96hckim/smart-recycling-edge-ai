"""
stream/socket_server.py

관제 PC 연동 고속 TCP 스트리밍 서버 (JPEG 영상 송신 + JSON 제어 명령 수신)
"""

import json
import socket
import struct
from typing import Any

import cv2
import numpy as np


class StreamSocketServer:
    """영상/메타데이터 송신 및 PC 제어 명령을 수신하는 TCP 서버"""

    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 9000,
        jpeg_quality: int = 70,
        timeout: float = 1.0,
    ):
        self.host = host
        self.port = port
        self.jpeg_quality = jpeg_quality
        self.timeout = timeout

        self._encode_params = (cv2.IMWRITE_JPEG_QUALITY, self.jpeg_quality)

        self.server_socket: socket.socket | None = None
        self.client_socket: socket.socket | None = None
        self.client_addr: tuple | None = None
        self._rx_buffer: str = ""  # 클라이언트 수신 데이터 버퍼

        self._init_server_socket()

    def _init_server_socket(self):
        """서버 소켓 생성 및 포트 즉시 재사용(SO_REUSEADDR) 설정"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.server_socket.settimeout(self.timeout)
        print(f"[NET] TCP 서버 바인딩 완료 -> {self.host}:{self.port}")

    @property
    def is_connected(self) -> bool:
        """클라이언트 연결 여부 반환"""
        return self.client_socket is not None

    def accept_client(self) -> bool:
        """관제 PC 클라이언트 접속 대기 (Non-blocking)"""
        if self.is_connected or self.server_socket is None:
            return False

        try:
            client, addr = self.server_socket.accept()
            client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            client.setblocking(False)  # 명령 수신 대기 렉 방지용 논블로킹 전환

            self.client_socket = client
            self.client_addr = addr
            self._rx_buffer = ""
            print(f"[NET] 관제 PC 연결 수락: {addr}")
            return True

        except (TimeoutError, BlockingIOError):
            return False
        except OSError as e:
            print(f"[NET ERROR] 클라이언트 연결 실패: {e}")
            return False

    def receive_command(self) -> dict[str, Any] | None:
        """클라이언트가 보낸 개행(\n) 단위의 JSON 제어 명령 논블로킹 파싱"""
        if not self.is_connected or self.client_socket is None:
            return None

        try:
            data = self.client_socket.recv(1024)
            if not data:
                print(f"[NET] 관제 PC({self.client_addr}) 정상 연결 종료")
                self.close_client()
                return None

            self._rx_buffer += data.decode("utf-8", errors="ignore")

            # 개행(\n) 기준으로 단일 완성 패킷 추출
            if "\n" in self._rx_buffer:
                line, self._rx_buffer = self._rx_buffer.split("\n", 1)
                line = line.strip()
                if line:
                    return json.loads(line)

        except (BlockingIOError, TimeoutError):
            pass
        except json.JSONDecodeError as e:
            print(f"[NET WARN] 비정상 JSON 수신: {e}")
        except (BrokenPipeError, ConnectionResetError):
            print(f"[NET] 관제 PC({self.client_addr}) 연결 끊김 감지")
            self.close_client()
        except OSError as e:
            print(f"[NET ERROR] 데이터 수신 실패: {e}")
            self.close_client()

        return None

    def send_frame(self, frame: np.ndarray, metadata: dict[str, Any]) -> bool:
        """
        [헤더(8B) + JPEG 영상 + JSON 메타데이터] 패킷 단일 전송
        - 헤더 구조: [이미지 크기(4B) + JSON 크기(4B)] (Big-Endian uint32)
        """
        if not self.is_connected or self.client_socket is None:
            return False

        try:
            # 1. 영상 JPEG 압축
            success, encimg = cv2.imencode(".jpg", frame, self._encode_params)
            if not success:
                return False
            img_bytes = encimg.tobytes()

            # 2. 메타데이터 직렬화
            json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")

            # 3. 8바이트 고정 헤더 패킹
            header = struct.pack(">II", len(img_bytes), len(json_bytes))

            # 4. 일괄 전송
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
        """연결된 클라이언트 소켓 해제 및 버퍼 정리"""
        if self.client_socket is not None:
            try:
                self.client_socket.close()
            except OSError:
                pass
            finally:
                self.client_socket = None
                self.client_addr = None
                self._rx_buffer = ""

    def close(self):
        """서버 소켓 및 클라이언트 연결 전체 해제"""
        self.close_client()
        if self.server_socket is not None:
            try:
                self.server_socket.close()
            except OSError:
                pass
            finally:
                self.server_socket = None
        print(f"[NET] 포트 {self.port} 소켓 정상 반환")

    def __del__(self):
        self.close()
