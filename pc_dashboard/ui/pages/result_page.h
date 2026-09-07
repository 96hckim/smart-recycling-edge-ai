/**
 * 분리배출 정산 결과, 탄소 절감량 숫자 롤링 및 자동 화면 복귀 타이머 헤더.
 */
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

    // 세션 통계 로드, 영수증 카드 렌더링, 수치 롤링 애니메이션 및 복귀 타이머 시작
    void showResult(const SessionSummary& summary);

signals:
    // 확인 버튼 클릭 또는 대기 타임아웃 만료 시 초기 화면 전환 시그널
    void sigReturnToIdleRequested();

private slots:
    void on_btnConfirm_clicked();
    void onCountdownTick();
    void onPointsAnimUpdate(const QVariant& value);
    void onCarbonAnimUpdate(const QVariant& value);

private:
    // 시각적 보상을 위한 축하 콘페티(폭죽) GIF 애니메이션 레이어 구성
    void initConfettiOverlay();
    // 무인 키오스크 방치 방지를 위한 초 단위 자동 복귀 카운트다운 타이머
    void setupTimer();
    // QVariantAnimation 기반 숫자 보간(Easing OutCubic) 애니메이션 초기화
    void setupAnimations();

    // 4대 품목별 투입 수량 및 획득 포인트 영수증 카드 UI 동적 갱신
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