#include "result_page.h"
#include "theme_constants.h"
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

    initConfettiOverlay();
    setupTimer();
    setupAnimations();
}

ResultPage::~ResultPage()
{
    delete ui;
}

void ResultPage::initConfettiOverlay()
{
    m_confettiMovie = new QMovie(UITheme::Result::CONFETTI_RESOURCE_PATH, QByteArray(), this);
    m_confettiMovie->setCacheMode(QMovie::CacheNone);
    m_confettiMovie->setSpeed(UITheme::Result::CONFETTI_SPEED);

    ui->lblConfetti->setFixedSize(UITheme::Result::CONFETTI_DISPLAY_SIZE);
    ui->lblConfetti->setScaledContents(true);
    ui->lblConfetti->setAlignment(Qt::AlignCenter);
    ui->lblConfetti->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->lblConfetti->setMovie(m_confettiMovie);
    ui->lblConfetti->lower();
}

void ResultPage::setupTimer()
{
    m_countdownTimer->setInterval(UITheme::Result::COUNTDOWN_INTERVAL_MS);
    connect(m_countdownTimer, &QTimer::timeout, this, &ResultPage::onCountdownTick);
}

void ResultPage::setupAnimations()
{
    m_pointsAnim->setDuration(UITheme::Result::ANIM_POINTS_DURATION_MS);
    m_pointsAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pointsAnim, &QVariantAnimation::valueChanged, this, &ResultPage::onPointsAnimUpdate);

    m_carbonAnim->setDuration(UITheme::Result::ANIM_CARBON_DURATION_MS);
    m_carbonAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_carbonAnim, &QVariantAnimation::valueChanged, this, &ResultPage::onCarbonAnimUpdate);
}

void ResultPage::showResult(const SessionSummary& summary)
{
    m_isMemberSession = summary.isMember;
    m_targetPoints = summary.totalPoints;
    m_targetCarbon = summary.totalCarbonG;

    // 1. 품목별 카드 갱신
    updateCard(ui->boxReceiptCan, ui->lblRCanTitle, ui->lblRCanCount, ui->lblRCanPoints,
        summary.canCount, Config::getPoint(RecycleCategory::CAN));
    updateCard(ui->boxReceiptPet, ui->lblRPetTitle, ui->lblRPetCount, ui->lblRPetPoints,
        summary.petCount, Config::getPoint(RecycleCategory::PET));
    updateCard(ui->boxReceiptPaper, ui->lblRPaperTitle, ui->lblRPaperCount, ui->lblRPaperPoints,
        summary.paperCount, Config::getPoint(RecycleCategory::PAPER));
    updateCard(ui->boxReceiptGeneral, ui->lblRGeneralTitle, ui->lblRGeneralCount, ui->lblRGeneralPoints,
        summary.generalCount, Config::getPoint(RecycleCategory::GENERAL));

    // 2. 사용자 알림 뱃지
    ui->lblUserNotice->setProperty(UITheme::PROP_MEMBER, summary.isMember);
    if (summary.isMember) {
        const QString name = summary.userName.isEmpty() ? UITheme::Recycle::Text::DEFAULT_MEMBER_NAME : summary.userName;
        ui->lblUserNotice->setText(QString(UITheme::Result::Text::NOTICE_MEMBER_FMT).arg(name));
    } else {
        ui->lblUserNotice->setText(UITheme::Result::Text::NOTICE_GUEST);
    }
    ui->lblUserNotice->style()->unpolish(ui->lblUserNotice);
    ui->lblUserNotice->style()->polish(ui->lblUserNotice);

    // 3. 축하 폭죽 1회 재생
    if (m_confettiMovie && m_confettiMovie->isValid()) {
        m_confettiMovie->stop();
        m_confettiMovie->jumpToFrame(0);
        m_confettiMovie->start();
    }

    // 4. 숫자 롤링 애니메이션
    m_pointsAnim->stop();
    m_carbonAnim->stop();

    if (m_isMemberSession) {
        m_pointsAnim->setStartValue(0);
        m_pointsAnim->setEndValue(m_targetPoints);
        m_pointsAnim->start();
    } else {
        ui->lblTotalPoints->setText(UITheme::Result::Text::POINTS_GUEST);
    }

    m_carbonAnim->setStartValue(0.0);
    m_carbonAnim->setEndValue(m_targetCarbon);
    m_carbonAnim->start();

    // 5. 카운트다운 타이머
    m_remainingSec = Config::RESULT_DISPLAY_TIMEOUT_SEC;
    ui->btnConfirm->setEnabled(true);
    ui->btnConfirm->setText(QString(UITheme::Result::Text::COUNTDOWN_BTN_FMT).arg(m_remainingSec));
    m_countdownTimer->start();
}

void ResultPage::onPointsAnimUpdate(const QVariant& value)
{
    if (m_isMemberSession) {
        ui->lblTotalPoints->setText(QString(UITheme::Result::Text::POINTS_PLUS_FMT).arg(value.toInt()));
    }
}

void ResultPage::onCarbonAnimUpdate(const QVariant& value)
{
    ui->lblTotalCarbon->setText(QString(UITheme::Result::Text::CARBON_FMT)
            .arg(QString::number(value.toDouble(), 'f', 1)));
}

void ResultPage::updateCard(QFrame* box, QLabel* lblTitle, QLabel* lblCount, QLabel* lblPoints,
    int count, int unitPoint)
{
    lblCount->setText(QString(UITheme::Result::Text::COUNT_UNIT_FMT).arg(count));

    if (count > 0) {
        lblPoints->setText(unitPoint > 0 ? QString(UITheme::Result::Text::POINTS_PLUS_FMT).arg(count * unitPoint)
                                         : UITheme::Result::Text::POINTS_ZERO);
    } else {
        lblPoints->setText(UITheme::Result::Text::EMPTY_DASH);
    }

    const bool isActive = (count > 0);
    const char* prop = UITheme::PROP_ACTIVE;

    box->setProperty(prop, isActive);
    lblTitle->setProperty(prop, isActive);
    lblCount->setProperty(prop, isActive);
    lblPoints->setProperty(prop, isActive);

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
        // if (m_confettiMovie)
        // m_confettiMovie->stop();
        // emit sigReturnToIdleRequested();
    } else {
        ui->btnConfirm->setText(QString(UITheme::Result::Text::COUNTDOWN_BTN_FMT).arg(m_remainingSec));
    }
}

void ResultPage::on_btnConfirm_clicked()
{
    ui->btnConfirm->setEnabled(false);
    m_countdownTimer->stop();
    if (m_confettiMovie)
        m_confettiMovie->stop();
    emit sigReturnToIdleRequested();
}