"""TensorRT 10.x V3 비동기 실행 컨텍스트 기반 Zero-Allocation GPU 추론 모듈."""

from contextlib import suppress
from pathlib import Path

import numpy as np
import pycuda.autoinit  # noqa: F401
import pycuda.driver as cuda
import tensorrt as trt


class TensorRTEngine:
    """Host Pinned 메모리 및 CUDA 비동기 스트림 기반 초저지연 추론 클래스."""

    def __init__(self, engine_path: Path | str):
        """TensorRT 직렬화 엔진 로드 및 I/O 버퍼 주소 1회 바인딩."""
        self.engine_path = Path(engine_path)
        if not self.engine_path.exists():
            raise FileNotFoundError(f"[TRT ERROR] 엔진 파일 없음: {self.engine_path}")

        self.logger = trt.Logger(trt.Logger.WARNING)
        self.runtime = trt.Runtime(self.logger)

        with open(self.engine_path, "rb") as f:
            self.engine = self.runtime.deserialize_cuda_engine(f.read())

        if self.engine is None:
            raise RuntimeError("[TRT ERROR] 엔진 역직렬화 실패")

        self.context = self.engine.create_execution_context()
        self.stream = cuda.Stream()

        self._allocate_buffers()
        print(
            f"[TRT] 바인딩 완료 (Input: {self.input_shape}, Output: {self.output_shape})"
        )

    def _allocate_buffers(self):
        """추론 루프 내 메모리 재할당 오버헤드를 없애기 위한 Host/Device 고정 버퍼 사전 할당."""
        for i in range(self.engine.num_io_tensors):
            name = self.engine.get_tensor_name(i)
            shape = tuple(self.engine.get_tensor_shape(name))
            dtype = trt.nptype(self.engine.get_tensor_dtype(name))

            # DMA 전송 효율을 극대화하는 Page-locked(Pinned) 호스트 메모리 할당
            h_mem = cuda.pagelocked_empty(shape, dtype=dtype)
            d_mem = cuda.mem_alloc(h_mem.nbytes)

            # TensorRT 10 엔진 컨텍스트에 GPU 물리 주소 직접 등록 (Zero-Allocation)
            self.context.set_tensor_address(name, int(d_mem))

            if self.engine.get_tensor_mode(name) == trt.TensorIOMode.INPUT:
                self.input_name = name
                self.input_shape = shape
                self.h_input = h_mem
                self.d_input = d_mem
            else:
                self.output_name = name
                self.output_shape = shape
                self.h_output = h_mem
                self.d_output = d_mem

    def execute(self, input_data: np.ndarray) -> np.ndarray:
        """비동기 파이프라인(H2D 전송 -> V3 커널 추론 -> D2H 전송) 실행 및 스트림 동기화."""
        np.copyto(self.h_input, input_data)

        cuda.memcpy_htod_async(self.d_input, self.h_input, self.stream)
        self.context.execute_async_v3(stream_handle=self.stream.handle)
        cuda.memcpy_dtoh_async(self.h_output, self.d_output, self.stream)

        # GPU 연산 완료 대기 후 동기화된 Host 버퍼 반환
        self.stream.synchronize()
        return self.h_output

    def destroy(self):
        """CUDA 스트림 동기화 대기 및 할당된 GPU VRAM 자원 안전 해제."""
        if self.stream is not None:
            with suppress(cuda.Error, OSError, AttributeError):
                self.stream.synchronize()

        with suppress(cuda.Error, OSError, AttributeError):
            if hasattr(self, "d_input") and self.d_input is not None:
                self.d_input.free()
                self.d_input = None
            if hasattr(self, "d_output") and self.d_output is not None:
                self.d_output.free()
                self.d_output = None

        self.h_input = None
        self.h_output = None
        self.context = None
        self.engine = None
        self.stream = None

    def __del__(self):
        self.destroy()
