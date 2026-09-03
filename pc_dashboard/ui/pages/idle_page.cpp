#include "idle_page.h"
#include "qrcodegen.hpp"
#include "ui_idle_page.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>

using qrcodegen::QrCode;

IdlePage::IdlePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::IdlePage)
{
    ui->setupUi(this);

    // 시연/테스트 편의용 마우스 클릭 인터랙션 유지
    ui->lblQrCode->installEventFilter(this);
    ui->lblQrCode->setCursor(Qt::PointingHandCursor);

    initQrCode();
}

IdlePage::~IdlePage()
{
    delete ui;
}

void IdlePage::initQrCode()
{
    const QString defaultQrPayload = QString(Config::Auth::DEEPLINK_PAYLOAD_FMT)
                                         .arg(Config::Auth::DEEPLINK_SCHEME)
                                         .arg(Config::DEFAULT_BIN_ID);

    updateQrCode(defaultQrPayload);
}

void IdlePage::updateQrCode(const QString& qrData)
{
    QPixmap qrPixmap = generateQrPixmap(qrData,
        UITheme::Idle::QR_DISPLAY_SIZE,
        UITheme::Idle::QR_QUIET_ZONE_MODULES);
    ui->lblQrCode->setPixmap(qrPixmap);
}

QPixmap IdlePage::generateQrPixmap(const QString& text, int targetSize, int margin)
{
    const QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
    const int qrSize = qr.getSize();

    QPixmap pixmap(targetSize, targetSize);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    const int totalModules = qrSize + (margin * 2);
    const double moduleSize = static_cast<double>(targetSize) / totalModules;

    const double offsetX = (targetSize - (totalModules * moduleSize)) / 2.0;
    const double offsetY = (targetSize - (totalModules * moduleSize)) / 2.0;

    for (int y = 0; y < qrSize; ++y) {
        for (int x = 0; x < qrSize; ++x) {
            if (qr.getModule(x, y)) {
                const double rectX = offsetX + ((x + margin) * moduleSize);
                const double rectY = offsetY + ((y + margin) * moduleSize);
                painter.drawRect(QRectF(rectX, rectY, moduleSize, moduleSize));
            }
        }
    }

    return pixmap;
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