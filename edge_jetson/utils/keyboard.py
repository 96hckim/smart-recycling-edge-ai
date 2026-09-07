"""리눅스 터미널 환경 논블로킹(Non-blocking) 단일 키 입력 감지 모듈."""

import select
import sys
import termios
import tty


class NonBlockingKeyReader:
    """터미널 cbreak 모드 전환 및 엔터 없는 즉시 키 입력 리더."""

    def __init__(self):
        """표준 입력(stdin) 속성 백업 및 즉각 문자 입력을 위한 cbreak 모드 활성화."""
        self.is_tty = sys.stdin.isatty()
        self._restored = False
        self.old_settings = None
        self.fd = None

        if self.is_tty:
            try:
                self.fd = sys.stdin.fileno()
                self.old_settings = termios.tcgetattr(self.fd)
                # 엔터 대기 없이 단일 키 입력을 수신하도록 tty 모드 변경
                tty.setcbreak(self.fd)
            except (termios.error, OSError):
                self.is_tty = False

    def get_key(self) -> str | None:
        """select() 기반 0초 타임아웃 비차단 문자 폴링 (미입력 시 None)."""
        if not self.is_tty or self._restored:
            return None

        try:
            if select.select([sys.stdin], [], [], 0)[0]:
                return sys.stdin.read(1)
        except (ValueError, OSError):
            return None
        return None

    def restore(self):
        """터미널 설정을 원래 canonical 모드로 복원 (중복 호출 안전)."""
        if (
            self.is_tty
            and not self._restored
            and self.fd is not None
            and self.old_settings is not None
        ):
            try:
                termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
            except (termios.error, OSError):
                pass
            self._restored = True

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.restore()
