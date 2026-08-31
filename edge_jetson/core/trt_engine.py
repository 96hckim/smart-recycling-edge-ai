"""
TensorRT 8.x / 10.x CudaStream 비동기 엔진 래퍼 (안전한 자원 해제)
"""

import os
from contextlib import suppress
from pathlib import Path

import numpy as np
import pycuda.autoinit  # noqa
import pycuda.driver as cuda
import tensorrt as trt


class TensorRTEngine:
    """
    TensorRT 직렬화 엔진(.engine) 로드 및 Pinned Memory 기반 추론 래퍼
    """

    def __init__(self, engine_path: str | Path):
        self.engine_path = str(engine_path)
        if not os.path.exists(self.engine_path):
            raise FileNotFoundError(
                f"[TRT] 엔진 파일을 찾을 수 없습니다: {self.engine_path}"
            )

        self.logger = trt.Logger(trt.Logger.WARNING)
        print(f"[TRT] 엔진 로딩 중: {self.engine_path}")

        with open(self.engine_path, "rb") as f, trt.Runtime(self.logger) as runtime:
            self.engine = runtime.deserialize_cuda_engine(f.read())

        if self.engine is None:
            raise RuntimeError("[TRT] 엔진 역직렬화 실패")

        self.context = self.engine.create_execution_context()
        self.stream = cuda.Stream()

        self._allocate_buffers()
        print(
            f"[TRT] 바인딩 메모리 할당 완료 (Input: {self.input_shape}, Output: {self.output_shape})"
        )

    def _allocate_buffers(self):
        """Host Pinned Memory 및 Device Buffer 1회 사전 할당"""
        self.host_inputs: list[np.ndarray] = []
        self.cuda_inputs: list[cuda.DeviceAllocation] = []
        self.host_outputs: list[np.ndarray] = []
        self.cuda_outputs: list[cuda.DeviceAllocation] = []
        self.bindings: list[int] = []

        num_io = (
            self.engine.num_io_tensors
            if hasattr(self.engine, "num_io_tensors")
            else self.engine.num_bindings
        )

        for i in range(num_io):
            name = (
                self.engine.get_tensor_name(i)
                if hasattr(self.engine, "get_tensor_name")
                else self.engine.get_binding_name(i)
            )
            shape = (
                self.engine.get_tensor_shape(name)
                if hasattr(self.engine, "get_tensor_shape")
                else self.engine.get_binding_shape(i)
            )
            dtype = (
                self.engine.get_tensor_dtype(name)
                if hasattr(self.engine, "get_tensor_dtype")
                else self.engine.get_binding_dtype(i)
            )

            actual_shape = [1 if dim == -1 else dim for dim in shape]
            size = trt.volume(actual_shape)
            np_dtype = trt.nptype(dtype)

            host_mem = cuda.pagelocked_empty(size, np_dtype)
            cuda_mem = cuda.mem_alloc(host_mem.nbytes)
            self.bindings.append(int(cuda_mem))

            is_input = (
                self.engine.get_tensor_mode(name) == trt.TensorIOMode.INPUT
                if hasattr(self.engine, "get_tensor_mode")
                else self.engine.binding_is_input(i)
            )

            if is_input:
                self.host_inputs.append(host_mem)
                self.cuda_inputs.append(cuda_mem)
                self.input_name = name
                self.input_shape = actual_shape
            else:
                self.host_outputs.append(host_mem)
                self.cuda_outputs.append(cuda_mem)
                self.output_name = name
                self.output_shape = actual_shape

    def execute(self, host_input_array: np.ndarray) -> np.ndarray:
        """[H2D 복사 -> 커널 실행 -> D2H 복사 -> 동기화]"""
        np.copyto(self.host_inputs[0], host_input_array.ravel())
        cuda.memcpy_htod_async(self.cuda_inputs[0], self.host_inputs[0], self.stream)

        if hasattr(self.context, "set_tensor_address"):
            self.context.set_tensor_address(self.input_name, int(self.cuda_inputs[0]))
            self.context.set_tensor_address(self.output_name, int(self.cuda_outputs[0]))
            self.context.execute_async_v3(stream_handle=self.stream.handle)
        else:
            self.context.execute_async_v2(
                bindings=self.bindings, stream_handle=self.stream.handle
            )

        cuda.memcpy_dtoh_async(self.host_outputs[0], self.cuda_outputs[0], self.stream)
        self.stream.synchronize()

        return self.host_outputs[0].reshape(self.output_shape)

    def destroy(self):
        """하위 의존성 순서에 맞춰 안전하게 객체 소멸 유도"""
        if hasattr(self, "stream") and self.stream:
            with suppress(cuda.Error, OSError, AttributeError):
                self.stream.synchronize()

        self.context = None
        self.engine = None
        self.stream = None
        self.cuda_inputs.clear()
        self.cuda_outputs.clear()

    def __del__(self):
        self.destroy()
