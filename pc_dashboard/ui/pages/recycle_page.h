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

namespace Ui
{
    class RecyclePage;
}

class EcoTreeController;

class RecyclePage : public QWidget
{
    Q_OBJECT

public:
    explicit RecyclePage(QWidget *parent = nullptr);
    ~RecyclePage() override;

    void startSession(bool isMember, const QString &userName = QString());
    void resetState();

    void updateFrame(const QPixmap &pixmap);
    void updateDetectionState(const QString &className, double confidence, int debounceCount, const QRect &box = QRect());
    void updateSessionSummary(const SessionSummary &summary);
    void setGuideBanner(UITheme::Recycle::BannerType type, const QString &customText = QString());

    // [추가] 도어 개폐 상태 갱신 인터페이스
    void updateDoorState(const HardwareDoorStatus &door);

signals:
    void sigFinishSessionRequested();
    void sigCancelSessionRequested();

private slots:
    void on_btnFinishSession_clicked();
    void on_btnCancelSession_clicked();

private:
    void applyDynamicProperty(QWidget *widget, const char *propName, const QVariant &value);

private:
    Ui::RecyclePage *ui;
    EcoTreeController *m_ecoTree{nullptr};
    QRect m_detectionBox{};
    QColor m_boxColor{UITheme::Recycle::DEFAULT_BOX_COLOR};
    QString m_boxLabel{};
    bool m_isMember{false};
    QString m_userName{};

    // [추가] 도어 열림 상태 플래그 (배너 오버라이트 방지)
    bool m_isDoorOpen{false};

    // 렌더링 최적화용 캐시 폰트 및 메트릭스
    QFont m_badgeFont;
    QFontMetrics m_badgeFontMetrics;
};

#endif // RECYCLE_PAGE_H