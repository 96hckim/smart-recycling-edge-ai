#include "result_page.h"
#include "ui_result_page.h"
#include <QEasingCurve>
#include <QStyle>

ResultPage::ResultPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultPage)
    , m_countdownTimer(new QTimer(this))
    , m_pointsAnim(new QVariantAnimation(this))
    , m_carbonAnim(new QVariantAnimation(this))
{
    ui->setupUi(this);

    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &ResultPage::onCountdownTick);

    setupAnimations();
}

ResultPage::~ResultPage()
{
    delete ui;
}

void ResultPage::setupAnimations()
{
    m_pointsAnim->setDuration(900);
    m_pointsAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pointsAnim, &QVariantAnimation::valueChanged, this, &ResultPage::onPointsAnimUpdate);

    m_carbonAnim->setDuration(1100);
    m_carbonAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_carbonAnim, &QVariantAnimation::valueChanged, this, &ResultPage::onCarbonAnimUpdate);
}

void ResultPage::showResult(const SessionSummary& summary)
{
    m_isMemberSession = summary.isMember;
    m_targetPoints = summary.totalPoints;
    m_targetCarbon = summary.totalCarbonG;

    // 1. Config::getPoint() 유틸리티 함수로 품목별 단가 매핑
    updateCard(ui->boxReceiptCan, ui->lblRCanTitle, ui->lblRCanCount, ui->lblRCanPoints,
        summary.canCount, Config::getPoint(RecycleCategory::CAN));

    updateCard(ui->boxReceiptPet, ui->lblRPetTitle, ui->lblRPetCount, ui->lblRPetPoints,
        summary.petCount, Config::getPoint(RecycleCategory::PET));

    updateCard(ui->boxReceiptPaper, ui->lblRPaperTitle, ui->lblRPaperCount, ui->lblRPaperPoints,
        summary.paperCount, Config::getPoint(RecycleCategory::PAPER));

    updateCard(ui->boxReceiptGeneral, ui->lblRGeneralTitle, ui->lblRGeneralCount, ui->lblRGeneralPoints,
        summary.generalCount, Config::getPoint(RecycleCategory::GENERAL));

    // 2. 사용자 상태 안내 뱃지 갱신
    ui->lblUserNotice->setProperty("member", summary.isMember);
    if (summary.isMember) {
        const QString name = summary.userName.isEmpty() ? "회원" : summary.userName;
        ui->lblUserNotice->setText(QString("✅ %1 님의 계정으로 포인트가 안전하게 적립되었습니다.").arg(name));
    } else {
        ui->lblUserNotice->setText("ℹ️ 비회원 이용 세션 (모바일 앱 가입 후 스캔 시 포인트가 적립됩니다)");
    }
    ui->lblUserNotice->style()->unpolish(ui->lblUserNotice);
    ui->lblUserNotice->style()->polish(ui->lblUserNotice);

    // 3. 지표 롤링 카운팅 애니메이션 구동
    m_pointsAnim->stop();
    m_carbonAnim->stop();

    if (m_isMemberSession) {
        m_pointsAnim->setStartValue(0);
        m_pointsAnim->setEndValue(m_targetPoints);
        m_pointsAnim->start();
    } else {
        ui->lblTotalPoints->setText("0 P (비회원)");
    }

    m_carbonAnim->setStartValue(0.0);
    m_carbonAnim->setEndValue(m_targetCarbon);
    m_carbonAnim->start();

    // 4. 타이머 가동
    m_remainingSec = Config::RESULT_DISPLAY_TIMEOUT_SEC;
    ui->btnConfirm->setEnabled(true);
    ui->btnConfirm->setText(QString("확인 (%1초 후 처음으로 이동)").arg(m_remainingSec));
    m_countdownTimer->start();
}

void ResultPage::onPointsAnimUpdate(const QVariant& value)
{
    if (m_isMemberSession) {
        ui->lblTotalPoints->setText(QString("+ %1 P").arg(value.toInt()));
    }
}

void ResultPage::onCarbonAnimUpdate(const QVariant& value)
{
    ui->lblTotalCarbon->setText(QString("%1 g CO₂").arg(QString::number(value.toDouble(), 'f', 1)));
}

void ResultPage::updateCard(QFrame* box, QLabel* lblTitle, QLabel* lblCount, QLabel* lblPoints,
    int count, int unitPoint)
{
    lblCount->setText(QString("%1 개").arg(count));

    if (count > 0) {
        lblPoints->setText(unitPoint > 0 ? QString("+ %1 P").arg(count * unitPoint) : "0 P");
    } else {
        lblPoints->setText("-");
    }

    const bool isActive = (count > 0);

    box->setProperty("active", isActive);
    lblTitle->setProperty("active", isActive);
    lblCount->setProperty("active", isActive);
    lblPoints->setProperty("active", isActive);

    box->style()->unpolish(box);
    box->style()->polish(box);
    lblTitle->style()->unpolish(lblTitle);
    lblTitle->style()->polish(lblTitle);
    lblCount->style()->unpolish(lblCount);
    lblCount->style()->polish(lblCount);
    lblPoints->style()->unpolish(lblPoints);
    lblPoints->style()->polish(lblPoints);
}

void ResultPage::onCountdownTick()
{
    m_remainingSec--;
    if (m_remainingSec <= 0) {
        m_countdownTimer->stop();
        emit sigReturnToIdleRequested();
    } else {
        ui->btnConfirm->setText(QString("확인 (%1초 후 처음으로 이동)").arg(m_remainingSec));
    }
}

void ResultPage::on_btnConfirm_clicked()
{
    ui->btnConfirm->setEnabled(false);
    m_countdownTimer->stop();
    emit sigReturnToIdleRequested();
}