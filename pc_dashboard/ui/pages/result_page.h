#pragma once
#ifndef RESULT_PAGE_H
#define RESULT_PAGE_H

#include "app_config.h"
#include <QFrame>
#include <QLabel>
#include <QMovie>
#include <QTimer>
#include <QVariantAnimation>
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
    void onPointsAnimUpdate(const QVariant& value);
    void onCarbonAnimUpdate(const QVariant& value);

private:
    void initConfettiOverlay();
    void setupTimer();
    void setupAnimations();

    void updateCard(QFrame* box, QLabel* lblTitle, QLabel* lblCount, QLabel* lblPoints,
        int count, int unitPoint);

private:
    Ui::ResultPage* ui;
    QTimer* m_countdownTimer { nullptr };
    QVariantAnimation* m_pointsAnim { nullptr };
    QVariantAnimation* m_carbonAnim { nullptr };
    QMovie* m_confettiMovie { nullptr };

    int m_remainingSec { Config::RESULT_DISPLAY_TIMEOUT_SEC };
    bool m_isMemberSession { false };
    int m_targetPoints { 0 };
    double m_targetCarbon { 0.0 };
};

#endif // RESULT_PAGE_H