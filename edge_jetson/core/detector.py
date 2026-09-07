"""YOLOv11 TensorRT 입출력 전처리 및 C++ NMS 기반 객체 검출 모듈."""

from typing import Any

import cv2
import numpy as np

from core.trt_engine import TensorRTEngine


class YOLOv11Detector:
    """Letterbox 전처리 및 C++ 가속 NMS 후처리를 수행하는 검출 파이프라인 클래스."""

    def __init__(
        self,
        engine: TensorRTEngine,
        input_shape: tuple[int, int] = (640, 640),
        conf_thresh: float = 0.50,
        iou_thresh: float = 0.45,
        class_names: tuple[str, ...] = ("paper", "rock", "scissors"),
    ):
        """검출 하이퍼파라미터 설정 및 매 프레임 버퍼 재생성 방지용 캔버스 캐싱."""
        self.engine = engine
        self.input_shape = input_shape
        self.conf_thresh = conf_thresh
        self.iou_thresh = iou_thresh
        self.class_names = class_names

        # 패딩 캔버스 메모리 재할당 오버헤드를 막기 위한 114 Gray 템플릿 캐싱
        target_w, target_h = self.input_shape
        self._canvas_template = np.full((target_h, target_w, 3), 114, dtype=np.uint8)

    def preprocess(self, img: np.ndarray) -> tuple[np.ndarray, dict[str, Any]]:
        """종횡비 유지 Letterbox 패딩 및 NCHW 정규화 Blob 생성."""
        orig_h, orig_w = img.shape[:2]
        target_w, target_h = self.input_shape

        scale = min(target_w / orig_w, target_h / orig_h)
        nw, nh = int(orig_w * scale), int(orig_h * scale)

        resized = cv2.resize(img, (nw, nh), interpolation=cv2.INTER_LINEAR)

        # 중앙 정렬 패딩
        canvas = self._canvas_template.copy()
        pad_w = (target_w - nw) // 2
        pad_h = (target_h - nh) // 2
        canvas[pad_h : pad_h + nh, pad_w : pad_w + nw] = resized

        # BGR->RGB 변환, 0~1 정규화, HWC->NCHW 전치를 단일 C++ 루틴으로 고속 처리
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
        """전처리 -> TensorRT 엔진 추론 -> NMS 후처리 단일 파이프라인 실행."""
        blob, meta = self.preprocess(img)
        raw_output = self.engine.execute(blob)
        return self._postprocess(raw_output, meta)

    def _postprocess(
        self, output: np.ndarray, meta: dict[str, Any]
    ) -> list[dict[str, Any]]:
        """신뢰도 벡터 마스킹, 원본 좌표계 역변환 및 OpenCV C++ NMS 적용."""
        preds = np.squeeze(output)
        if preds.shape[0] < preds.shape[1]:
            preds = preds.T

        scores = preds[:, 4:]
        confidences = np.max(scores, axis=1)
        class_ids = np.argmax(scores, axis=1)

        # 8,400개 앵커 박스 전체 연산 낭비를 줄이기 위한 신뢰도 1차 벡터 필터링
        mask = confidences >= self.conf_thresh
        if not np.any(mask):
            return []

        filtered_boxes = preds[mask, :4]
        filtered_conf = confidences[mask]
        filtered_cls = class_ids[mask]

        scale = meta["scale"]
        pad_w, pad_h = meta["pad_w"], meta["pad_h"]
        orig_w, orig_h = meta["orig_w"], meta["orig_h"]

        # 캔버스 중심 좌표계(cx, cy, w, h) -> 원본 영상 좌표계(x1, y1, x2, y2) 일괄 복원
        cx, cy = filtered_boxes[:, 0], filtered_boxes[:, 1]
        w, h = filtered_boxes[:, 2], filtered_boxes[:, 3]

        x1 = np.clip((cx - w / 2.0 - pad_w) / scale, 0, orig_w)
        y1 = np.clip((cy - h / 2.0 - pad_h) / scale, 0, orig_h)
        x2 = np.clip((cx + w / 2.0 - pad_w) / scale, 0, orig_w)
        y2 = np.clip((cy + h / 2.0 - pad_h) / scale, 0, orig_h)

        boxes_for_nms = (
            np.stack([x1, y1, x2 - x1, y2 - y1], axis=1).astype(int).tolist()
        )
        confs_list = filtered_conf.tolist()
        cls_ids_list = filtered_cls.tolist()

        # Python 이중 루프 대비 수십 배 빠른 OpenCV C++ 바인딩 NMS 사용
        indices = cv2.dnn.NMSBoxes(
            boxes_for_nms, confs_list, self.conf_thresh, self.iou_thresh
        )

        detections: list[dict[str, Any]] = []
        if len(indices) > 0:
            for idx in np.array(indices).flatten():
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
                        "box": [bx, by, bx + bw, by + bh],
                    }
                )

        return detections
