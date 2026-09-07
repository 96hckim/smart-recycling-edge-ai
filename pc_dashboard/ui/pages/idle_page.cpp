/**
 * QPainter 기반 비트맵 QR 생성 및 시연용 터치 이벤트 처리 구현부.
 */
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

    // 실제 키오스크 터치스크린 환경 및 마우스 시연을 위한 이벤트 필터 부착
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
    // 스마트폰 카메라 인식률을 보장하는 중간 수준(Medium) 오류 정정 부호 적용
    const QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
    const int qrSize = qr.getSize();

    QPixmap pixmap(targetSize, targetSize);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    // 모듈 경계 번짐으로 인한 QR 스캔 실패를 방지하기 위해 앤티에일리어싱 해제
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    // 라벨 크기 대비 콰이어트 존(여백)과 모듈 크기를 정밀 계산하여 중앙 정렬
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
    // 현장 시연 및 앱 미연동 환경 테스트를 위한 QR 영역 직접 클릭 바이패스
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