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

    void updateQrCode(const QString& qrData);

signals:
    void sigMemberStartRequested(const QString& userId = Config::Demo::MEMBER_USER_ID);
    void sigGuestStartRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_btnGuestStart_clicked();

private:
    void initQrCode();
    QPixmap generateQrPixmap(const QString& text,
        int targetSize = UITheme::Idle::QR_DISPLAY_SIZE,
        int margin = UITheme::Idle::QR_QUIET_ZONE_MODULES);

private:
    Ui::IdlePage* ui;
};

#endif // IDLE_PAGE_H