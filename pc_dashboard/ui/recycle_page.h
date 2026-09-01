#ifndef RECYCLE_PAGE_H
#define RECYCLE_PAGE_H

#include "app_config.h"
#include <QPixmap>
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
    void updateFrame(const QPixmap& pixmap);
    void updateDetectionState(const QString& className, double confidence, int debounceCount);
    void updateSessionSummary(int canCount, int petCount, int paperCount, int generalCount, int totalPoints, double totalCarbon);
    void resetState();

signals:
    void sigFinishSessionRequested();
    void sigCancelSessionRequested();

private slots:
    void on_btnFinishSession_clicked();
    void on_btnCancelSession_clicked();

private:
    Ui::RecyclePage* ui;
    bool m_isMember { false };
    QString m_userName;
};

#endif // RECYCLE_PAGE_H