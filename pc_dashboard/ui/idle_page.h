#ifndef IDLE_PAGE_H
#define IDLE_PAGE_H

#include <QWidget>

namespace Ui {
class IdlePage;
}

class IdlePage : public QWidget {
    Q_OBJECT

public:
    explicit IdlePage(QWidget* parent = nullptr);
    ~IdlePage() override;

signals:
    // 모바일 앱 QR 스캔 또는 시연용 터치 시 발생
    void sigMemberStartRequested(const QString& userId = "MEMBER_DEMO_USER");

    // 비회원 바로 시작 버튼 클릭 시 발생
    void sigGuestStartRequested();

protected:
    // PC 단독 테스트 및 시연용 마우스 클릭 이벤트 필터
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_btnGuestStart_clicked();

private:
    Ui::IdlePage* ui;
};

#endif // IDLE_PAGE_H