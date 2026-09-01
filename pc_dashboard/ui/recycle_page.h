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

    // 세션 시작 (회원/비회원 통합 모드)
    void startSession(bool isMember, const QString& userName = QString());

    // 실시간 카메라 비디오 프레임 갱신
    void updateFrame(const QPixmap& pixmap);

    // AI 실시간 인식 클래스 및 5프레임 Debounce 게이지 갱신
    void updateDetectionState(const QString& className, double confidence, int debounceCount);

    // 4종 품목 누적 수량 및 리워드 스코어보드 갱신
    void updateSessionSummary(int canCount, int petCount, int paperCount, int generalCount,
        int totalPoints, double totalCarbon);

    // 투입 세션 종료 시 화면 상태 초기화
    void resetState();

signals:
    // 투입 완료 버튼 클릭 (결과 화면 이동)
    void sigFinishSessionRequested();

    // 투입 취소 버튼 클릭 (대기 화면 복귀)
    void sigCancelSessionRequested();

private slots:
    void on_btnFinishSession_clicked();
    void on_btnCancelSession_clicked();

private:
    // 하단 AI 가이드 배너 텍스트 및 스타일 갱신 헬퍼
    void setGuideBanner(const QString& text, const QString& textColor,
        const QString& borderColor, const QString& bgColor);

private:
    Ui::RecyclePage* ui;
    bool m_isMember { false };
    QString m_userName;
};

#endif // RECYCLE_PAGE_H