#pragma once
#ifndef VISION_OVERLAY_H
#define VISION_OVERLAY_H

#include "app_config.h"
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <algorithm>

class VisionOverlay {
public:
    static void drawDetections(QPixmap& pixmap, const QVector<Detection>& detections)
    {
        if (pixmap.isNull() || detections.isEmpty())
            return;

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font("Pretendard", 11, QFont::Bold);
        painter.setFont(font);

        const QRect imgBounds = pixmap.rect();

        for (const auto& det : detections) {
            if (det.box.isEmpty())
                continue;

            // 1. 영상 경계 내로 사각 박스 보정
            QRect box = det.box.intersected(imgBounds);
            if (box.width() < 10 || box.height() < 10)
                continue;

            const QColor themeColor = Config::getCategoryColor(det.category);

            // 2. 바운딩 박스 채우기(반투명) 및 테두리 렌더링
            QColor fillColor = themeColor;
            fillColor.setAlpha(25);
            painter.fillRect(box, fillColor);

            QPen pen(themeColor, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawRoundedRect(box, 6, 6);

            // 3. 태그 라벨 조립
            QString displayCategory = Config::getCategoryNameEn(det.category);
            QString labelText;
            if constexpr (Config::USE_MOCK_RPS_MODEL) {
                labelText = QString("%1 [%2] %3%")
                                .arg(displayCategory,
                                    det.className.toUpper(),
                                    QString::number(det.confidence * 100.0, 'f', 0));
            } else {
                labelText = QString("%1 %2%")
                                .arg(displayCategory,
                                    QString::number(det.confidence * 100.0, 'f', 0));
            }

            // 4. 배지 크기 및 위치 계산
            QFontMetrics fm(font);
            const int textWidth = fm.horizontalAdvance(labelText);
            const int textHeight = fm.height();
            const int padX = 8;
            const int padY = 4;
            const int badgeW = textWidth + (padX * 2);
            const int badgeH = textHeight + (padY * 2);

            int badgeY = box.top() - badgeH - 3;
            if (badgeY < imgBounds.top()) {
                badgeY = box.top() + 3;
            }
            int badgeX = std::clamp(box.left(), imgBounds.left() + 2, imgBounds.right() - badgeW - 2);

            QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

            // 5. 배지 드로잉
            QColor badgeBg = themeColor;
            badgeBg.setAlpha(220);
            painter.setPen(Qt::NoPen);
            painter.setBrush(badgeBg);
            painter.drawRoundedRect(badgeRect, 5, 5);

            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, labelText);
        }
    }
};

#endif // VISION_OVERLAY_H