"""
core/trt_engine.py

TensorRT 10.x 전용 Zero-Allocation 비동기 GPU 추론 엔진
"""

from contextlib import suppress
from pathlib import Path

import numpy as np
import pycuda.autoinit  # noqa: F401
import pycuda.driver as cuda
import tensorrt as trt


class TensorRTEngine:
    """TensorRT 10 V3 Execution Context 기반 고속 비동기 추론 클래스"""

    def __init__(self, engine_path: Path | str):
        self.engine_path = Path(engine_path)
        if not self.engine_path.exists():
            raise FileNotFoundError(f"[TRT ERROR] 엔진 파일 없음: {self.engine_path}")

        self.logger = trt.Logger(trt.Logger.WARNING)
        self.runtime = trt.Runtime(self.logger)

        # 1. 엔진 역직렬화 및 실행 컨텍스트 생성
        with open(self.engine_path, "rb") as f:
            self.engine = self.runtime.deserialize_cuda_engine(f.read())

        if self.engine is None:
            raise RuntimeError("[TRT ERROR] 엔진 역직렬화 실패")

        self.context = self.engine.create_execution_context()
        self.stream = cuda.Stream()

        # 2. Host/Device Pinned Buffer 사전 할당 및 텐서 주소 1회 바인딩
        self._allocate_buffers()
        print(
            f"[TRT] 바인딩 완료 (Input: {self.input_shape}, Output: {self.output_shape})"
        )

    def _allocate_buffers(self):
        """텐서별 Pinned Memory 사전 할당 및 주소 1회 사전 바인딩 (Zero-Allocation)"""
        for i in range(self.engine.num_io_tensors):
            name = self.engine.get_tensor_name(i)
            shape = tuple(self.engine.get_tensor_shape(name))
            dtype = trt.nptype(self.engine.get_tensor_dtype(name))

            # Page-Locked(Pinned) Host 메모리 & GPU Device 메모리 할당
            h_mem = cuda.pagelocked_empty(shape, dtype=dtype)
            d_mem = cuda.mem_alloc(h_mem.nbytes)

            # TensorRT 10 엔진에 GPU 버퍼 주소 1회 사전 등록
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
        """비동기 파이프라인: H2D 복사 -> GPU 커널 실행 -> D2H 복사 -> 스트림 동기화"""
        # 1. 사전 할당된 Pinned Memory로 입력 복사
        np.copyto(self.h_input, input_data)

        # 2. 비동기 전송 및 추론 실행 (주소 재바인딩 없이 즉각 실행)
        cuda.memcpy_htod_async(self.d_input, self.h_input, self.stream)
        self.context.execute_async_v3(stream_handle=self.stream.handle)
        cuda.memcpy_dtoh_async(self.h_output, self.d_output, self.stream)

        # 3. 동기화 후 고정 출력 버퍼 반환 (추가 메모리 할당 0byte)
        self.stream.synchronize()
        return self.h_output

    def destroy(self):
        """CUDA 스트림 동기화 및 리소스 안전 해제"""
        if self.stream is not None:
            with suppress(cuda.Error, OSError, AttributeError):
                self.stream.synchronize()

        self.context = None
        self.engine = None
        self.stream = None

    def __del__(self):
        self.destroy()
