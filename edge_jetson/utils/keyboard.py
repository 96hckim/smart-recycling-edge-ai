"""
utils/keyboard.py

리눅스 터미널 비차단(Non-blocking) 단일 키 입력 감지 모듈
"""

import select
import sys
import termios
import tty


class NonBlockingKeyReader:
    """엔터 입력 없이 키보드 입력을 즉시 감지하는 비차단 리더"""

    def __init__(self):
        self.fd = sys.stdin.fileno()
        self.old_settings = termios.tcgetattr(self.fd)
        self._restored = False

        # 엔터 없이 즉시 단일 문자를 읽도록 cbreak 모드로 전환
        tty.setcbreak(self.fd)

    def get_key(self) -> str | None:
        """입력 대기열에 키가 있으면 즉시 반환, 없으면 None 반환"""
        if self._restored:
            return None

        # sys.stdin 대기 상태 확인 (타임아웃 0초 = 즉시 반환)
        if select.select([sys.stdin], [], [], 0)[0]:
            return sys.stdin.read(1)
        return None

    def restore(self):
        """터미널 설정을 원래 표준 모드로 복구 (중복 호출 안전)"""
        if not self._restored:
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
            self._restored = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.restore()
