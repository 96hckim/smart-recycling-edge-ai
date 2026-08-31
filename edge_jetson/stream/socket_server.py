"""
PC 대시보드 연동 고속 TCP 스트리밍 서버 (JPEG + JSON 바이너리 패킹)
"""

import json
import socket
import struct
from typing import Any

import cv2
import numpy as np


class StreamSocketServer:
    """
    영상 프레임(JPEG)과 검출 메타데이터(JSON)를 8바이트 헤더 패킷으로
    직렬화하여 PC 클라이언트로 고속 송신하는 TCP 서버 클래스
    """

    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 8080,
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

        self._init_server_socket()

    def _init_server_socket(self):
        """서버 소켓 생성 및 포트 즉각 재사용/공유 옵션 강제 적용"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # 1. TIME_WAIT 포트 즉시 재사용
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # 2. 동일 포트 바인딩 충돌 방지 (Linux 커널 옵션)
        if hasattr(socket, "SO_REUSEPORT"):
            try:
                self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            except OSError:
                pass

        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.server_socket.settimeout(self.timeout)
        print(f"[NET] TCP 서버 바인딩 완료 -> {self.host}:{self.port}")

    @property
    def is_connected(self) -> bool:
        """현재 유효한 클라이언트가 연결되어 있는지 여부"""
        return self.client_socket is not None

    def accept_client(self) -> bool:
        """
        클라이언트 접속 대기 (타임아웃 적용 및 로컬 루프백 프로빙 차단)
        """
        if self.is_connected or self.server_socket is None:
            return False

        try:
            client, addr = self.server_socket.accept()
            client_ip = addr[0]

            # [근본 차단] 127.0.0.1(VS Code 헬스체크 / 로컬 루프백) 즉시 드랍
            if client_ip in ("127.0.0.1", "localhost", "::1"):
                try:
                    client.shutdown(socket.SHUT_RDWR)
                    client.close()
                except OSError:
                    pass
                return False

            # 외부 관제 PC 정식 연결 수락
            client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            client.settimeout(self.timeout)

            self.client_socket = client
            self.client_addr = addr
            print(f"[NET] 관제 PC 대시보드 연결 성공: {addr}")
            return True

        except TimeoutError:
            return False
        except OSError as e:
            print(f"[NET ERROR] 연결 수락 중 소켓 오류: {e}")
            return False

    def send_frame(self, frame: np.ndarray, metadata: dict[str, Any]) -> bool:
        """
        프레임 압축 및 메타데이터 직렬화 후 [헤더(8B) + 이미지 + JSON] 일괄 전송

        :param frame: 원본 BGR 이미지
        :param metadata: FPS, 추론시간, Bounding Box 좌표 등이 담긴 딕셔너리
        :return: 전송 성공 여부
        """
        if not self.is_connected or self.client_socket is None:
            return False

        try:
            # 1. 원본 BGR -> JPEG 압축 인코딩
            success, encimg = cv2.imencode(".jpg", frame, self._encode_params)
            if not success:
                return False
            img_bytes = encimg.tobytes()

            # 2. 메타데이터 JSON 직렬화
            json_bytes = json.dumps(metadata, ensure_ascii=False).encode("utf-8")

            # 3. 8바이트 고정 헤더 패킹 (ImgLen: 4B, JsonLen: 4B, Big-Endian)
            header = struct.pack(">II", len(img_bytes), len(json_bytes))

            # 4. 일괄 전송
            self.client_socket.sendall(header + img_bytes + json_bytes)
            return True

        except (TimeoutError, BrokenPipeError, ConnectionResetError):
            print(f"[NET] PC{self.client_addr} 연결 끊김 감지")
            self.close_client()
            return False
        except OSError as e:
            print(f"[NET ERROR] 패킷 전송 실패: {e}")
            self.close_client()
            return False

    def close_client(self):
        """연결된 클라이언트 소켓 안전 해제"""
        if self.client_socket:
            try:
                self.client_socket.shutdown(socket.SHUT_RDWR)
                self.client_socket.close()
            except OSError:
                pass
            finally:
                self.client_socket = None
                self.client_addr = None

    def close(self):
        """서버 및 클라이언트 소켓 전체 해제 (중복 호출 방지)"""
        # 이미 닫힌 상태라면 중복 실행 방지
        if self.server_socket is None and self.client_socket is None:
            return

        self.close_client()
        if self.server_socket:
            try:
                self.server_socket.shutdown(socket.SHUT_RDWR)
                self.server_socket.close()
            except OSError:
                pass
            finally:
                self.server_socket = None
        print(f"[NET] 포트 {self.port} 해제 및 서버 정상 종료")

    def __del__(self):
        self.close()
