"""
YOLOv11 전/후처리 및 바운딩 박스 검출기 (NumPy 벡터 최적화)
"""

from typing import Any

import cv2
import numpy as np
from core.trt_engine import TensorRTEngine


class YOLOv11Detector:
    def __init__(
        self,
        engine: TensorRTEngine,
        input_shape: tuple[int, int] = (640, 640),
        conf_thresh: float = 0.50,
        iou_thresh: float = 0.45,
        class_names: tuple[str, ...] = ("paper", "rock", "scissors"),
    ):
        self.engine = engine
        self.input_shape = input_shape
        self.conf_thresh = conf_thresh
        self.iou_thresh = iou_thresh
        self.class_names = class_names

        # 캔버스 버퍼 1회 사전 할당 (GC 부하 제거)
        target_w, target_h = self.input_shape
        self._canvas_template = np.full((target_h, target_w, 3), 114, dtype=np.uint8)

    def preprocess(self, img: np.ndarray) -> tuple[np.ndarray, dict[str, Any]]:
        orig_h, orig_w = img.shape[:2]
        target_w, target_h = self.input_shape

        scale = min(target_w / orig_w, target_h / orig_h)
        nw, nh = int(orig_w * scale), int(orig_h * scale)

        resized = cv2.resize(img, (nw, nh), interpolation=cv2.INTER_LINEAR)

        # 템플릿 복사 후 슬라이스 삽입
        canvas = self._canvas_template.copy()
        pad_w = (target_w - nw) // 2
        pad_h = (target_h - nh) // 2
        canvas[pad_h : pad_h + nh, pad_w : pad_w + nw] = resized

        # OpenCV C++ 커널 가속 (BGR->RGB, 0~1 정규화, HWC->NCHW 전치를 1번에 수행)
        blob = cv2.dnn.blobFromImage(
            canvas,
            scalefactor=1.0 / 255.0,
            size=(target_w, target_h),
            mean=(0, 0, 0),
            swapRB=True,
            crop=False,
        )

        meta = {
            "scale": scale,
            "pad_w": pad_w,
            "pad_h": pad_h,
            "orig_w": orig_w,
            "orig_h": orig_h,
        }
        return blob, meta

    def detect(self, img: np.ndarray) -> list[dict[str, Any]]:
        """
        이미지 입력 -> 전처리 -> TRT 추론 -> 후처리 파이프라인 단일 호출 메서드
        """
        blob, meta = self.preprocess(img)
        raw_output = self.engine.execute(blob)
        return self._postprocess(raw_output, meta)

    def _postprocess(
        self, output: np.ndarray, meta: dict[str, Any]
    ) -> list[dict[str, Any]]:
        """
        YOLOv11 출력 텐서 -> NumPy 벡터 필터링 -> NMS -> 최종 검출 결과 리스트 반환
        """
        # 출력 형상 정규화: (1, 4 + N, 8400) 또는 (4 + N, 8400) -> (8400, 4 + N)
        preds = np.squeeze(output)
        if preds.shape[0] < preds.shape[1]:
            preds = preds.T

        # 클래스 스코어 및 최고 확률 클래스 추출
        scores = preds[:, 4:]
        confidences = np.max(scores, axis=1)
        class_ids = np.argmax(scores, axis=1)

        # 1. 신뢰도 기준 1차 벡터 마스킹 (루프 없이 8400개 일괄 필터링)
        mask = confidences >= self.conf_thresh
        if not np.any(mask):
            return []

        filtered_boxes = preds[mask, :4]
        filtered_conf = confidences[mask]
        filtered_cls = class_ids[mask]

        scale = meta["scale"]
        pad_w, pad_h = meta["pad_w"], meta["pad_h"]
        orig_w, orig_h = meta["orig_w"], meta["orig_h"]

        # 2. 중심 좌표/크기(cx, cy, w, h) -> 원본 좌표계 좌상단/우하단(x1, y1, x2, y2) 일괄 역산
        cx = filtered_boxes[:, 0]
        cy = filtered_boxes[:, 1]
        w = filtered_boxes[:, 2]
        h = filtered_boxes[:, 3]

        x1 = np.clip((cx - w / 2.0 - pad_w) / scale, 0, orig_w)
        y1 = np.clip((cy - h / 2.0 - pad_h) / scale, 0, orig_h)
        x2 = np.clip((cx + w / 2.0 - pad_w) / scale, 0, orig_w)
        y2 = np.clip((cy + h / 2.0 - pad_h) / scale, 0, orig_h)

        # OpenCV NMS 입력 포맷: [x, y, width, height]
        boxes_for_nms = (
            np.stack([x1, y1, x2 - x1, y2 - y1], axis=1).astype(int).tolist()
        )
        confs_list = filtered_conf.tolist()
        cls_ids_list = filtered_cls.tolist()

        # 3. OpenCV C++ 고속 NMS 실행
        indices = cv2.dnn.NMSBoxes(
            boxes_for_nms, confs_list, self.conf_thresh, self.iou_thresh
        )

        detections: list[dict[str, Any]] = []
        if len(indices) > 0:
            for idx in indices.flatten():
                bx, by, bw, bh = boxes_for_nms[idx]
                cid = int(cls_ids_list[idx])
                cname = (
                    self.class_names[cid]
                    if cid < len(self.class_names)
                    else f"class_{cid}"
                )

                detections.append(
                    {
                        "class_id": cid,
                        "class_name": cname,
                        "confidence": round(confs_list[idx], 3),
                        "box": [bx, by, bx + bw, by + bh],  # [x1, y1, x2, y2]
                    }
                )

        return detections
