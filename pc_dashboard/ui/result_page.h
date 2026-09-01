#ifndef RESULT_PAGE_H
#define RESULT_PAGE_H

#include "app_config.h"
#include <QTimer>
#include <QWidget>

namespace Ui {
class ResultPage;
}

class ResultPage : public QWidget {
    Q_OBJECT

public:
    explicit ResultPage(QWidget* parent = nullptr);
    ~ResultPage() override;

    void showResult(const SessionSummary& summary);

signals:
    void sigReturnToIdleRequested();

private slots:
    void on_btnConfirm_clicked();
    void onCountdownTick();

private:
    Ui::ResultPage* ui;
    QTimer* m_countdownTimer { nullptr };
    int m_remainingSec { Config::RESULT_DISPLAY_TIMEOUT_SEC };
};

#endif // RESULT_PAGE_H