/**
 * 키오스크 대기 모드 UI 및 모바일 앱 딥링크 QR 렌더링 헤더.
 */
#pragma once
#ifndef IDLE_PAGE_H
#define IDLE_PAGE_H

#include "app_config.h"
#include "theme_constants.h"
#include <QPixmap>
#include <QWidget>

namespace Ui {
class IdlePage;
}

class IdlePage : public QWidget {
    Q_OBJECT

public:
    explicit IdlePage(QWidget* parent = nullptr);
    ~IdlePage() override;

    // 모바일 딥링크 규격에 맞춘 동적 QR 코드 비트맵 생성 및 화면 갱신
    void updateQrCode(const QString& qrData);

signals:
    // QR 인증 완료 또는 시연용 터치 감지 시 회원 배출 세션 개시 요청
    void sigMemberStartRequested(const QString& userId = Config::Demo::MEMBER_USER_ID);
    // 비회원 간편 투입 시작 요청
    void sigGuestStartRequested();

protected:
    // 터치스크린/마우스 클릭 인터랙션을 위한 QR 라벨 이벤트 필터
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_btnGuestStart_clicked();

private:
    // 키오스크 고유 식별자(Bin ID) 기반 초기 딥링크 QR 렌더링
    void initQrCode();
    // QrCode 생성 라이브러리 연동 및 QPainter 기반 픽셀 완충 QR Pixmap 렌더링
    QPixmap generateQrPixmap(const QString& text,
        int targetSize = UITheme::Idle::QR_DISPLAY_SIZE,
        int margin = UITheme::Idle::QR_QUIET_ZONE_MODULES);

private:
    Ui::IdlePage* ui;
};

#endif // IDLE_PAGE_H