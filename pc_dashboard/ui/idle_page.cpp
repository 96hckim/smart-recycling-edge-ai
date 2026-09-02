#include "idle_page.h"
#include "app_config.h"
#include "ui_idle_page.h"
#include <QEvent>
#include <QMouseEvent>

IdlePage::IdlePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::IdlePage)
{
    ui->setupUi(this);

    // 개발/시연 편의를 위해 QR 라벨에 마우스 포인터 핸드 커서 및 이벤트 필터 등록
    ui->lblQrCode->installEventFilter(this);
    ui->lblQrCode->setCursor(Qt::PointingHandCursor);
}

IdlePage::~IdlePage()
{
    delete ui;
}

bool IdlePage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->lblQrCode && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                emit sigMemberStartRequested(Config::Demo::MEMBER_USER_ID);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void IdlePage::on_btnGuestStart_clicked()
{
    emit sigGuestStartRequested();
}