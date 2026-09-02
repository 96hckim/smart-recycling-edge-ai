#ifndef RECYCLE_PAGE_H
#define RECYCLE_PAGE_H

#include "app_config.h"
#include <QPixmap>
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

    // 세션 시작 및 상태 초기화
    void startSession(bool isMember, const QString& userName = QString());
    void resetState();

    // 비전 스트림 및 AI 디텍션 UI 업데이트
    void updateFrame(const QPixmap& pixmap);
    void updateDetectionState(const QString& className, double confidence, int debounceCount);

    // 세션 정산 현황 업데이트 (SessionSummary 구조체 참조)
    void updateSessionSummary(const SessionSummary& summary);

signals:
    void sigFinishSessionRequested();
    void sigCancelSessionRequested();

private slots:
    void on_btnFinishSession_clicked();
    void on_btnCancelSession_clicked();

private:
    void setGuideBanner(const QString& text, const QString& textColor,
        const QString& borderColor, const QString& bgColor);

private:
    Ui::RecyclePage* ui;
    bool m_isMember { false };
    QString m_userName { };
};

#endif // RECYCLE_PAGE_H