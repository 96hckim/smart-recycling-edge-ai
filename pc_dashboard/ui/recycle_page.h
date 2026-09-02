#pragma once
#ifndef RECYCLE_PAGE_H
#define RECYCLE_PAGE_H

#include "app_config.h"
#include "theme_constants.h"
#include <QColor>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

namespace Ui {
class RecyclePage;
}

class RecyclePage : public QWidget {
    Q_OBJECT

public:
    explicit RecyclePage(QWidget* parent = nullptr);
    ~RecyclePage() override;

    void startSession(bool isMember, const QString& userName = QString());
    void resetState();

    void updateFrame(const QPixmap& pixmap);
    void updateDetectionState(const QString& className, double confidence, int debounceCount, const QRect& box = QRect());
    void updateSessionSummary(const SessionSummary& summary);

signals:
    void sigFinishSessionRequested();
    void sigCancelSessionRequested();

private slots:
    void on_btnFinishSession_clicked();
    void on_btnCancelSession_clicked();

private:
    void setGuideBanner(UITheme::BannerType type, const QString& customText = QString());
    void applyDynamicProperty(QWidget* widget, const char* propName, const QVariant& value);

private:
    Ui::RecyclePage* ui;
    QRect m_detectionBox { };
    QColor m_boxColor { "#10B981" };
    QString m_boxLabel { };
    bool m_isMember { false };
    QString m_userName { };
};

#endif // RECYCLE_PAGE_H