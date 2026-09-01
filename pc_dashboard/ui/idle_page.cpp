#include "idle_page.h"
#include "ui_idle_page.h"
#include <QEvent>
#include <QMouseEvent>

IdlePage::IdlePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::IdlePage)
{
    ui->setupUi(this);
    ui->lblQrCode->installEventFilter(this);
    ui->lblQrCode->setCursor(Qt::PointingHandCursor);
}

IdlePage::~IdlePage()
{
    delete ui;
}

bool IdlePage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->lblQrCode && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit sigMemberStartRequested("MEMBER_DEMO_USER");
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void IdlePage::on_btnGuestStart_clicked()
{
    emit sigGuestStartRequested();
}