#include "result_page.h"
#include "ui_result_page.h"

ResultPage::ResultPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultPage)
    , m_countdownTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 1초 단위 타이머 틱 연결
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &ResultPage::onCountdownTick);
}

ResultPage::~ResultPage()
{
    delete ui;
}

void ResultPage::showResult(const SessionSummary& summary)
{
    // 1. CAN (캔) 영수증 렌더링
    ui->lblRCanCount->setText(QString("%1 개").arg(summary.canCount));
    if (summary.canCount > 0) {
        ui->boxReceiptCan->setStyleSheet("QFrame#boxReceiptCan { background-color: #0E1A1A; border: 1.5px solid #10B981; border-radius: 16px; }");
        ui->lblRCanCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #10B981; background: transparent;");
        ui->lblRCanPoints->setText(QString("+ %1 P").arg(summary.canCount * Config::POINT_CAN));
        ui->lblRCanPoints->setStyleSheet("font-size: 15px; font-weight: 700; color: #10B981; background: transparent;");
    } else {
        ui->boxReceiptCan->setStyleSheet("QFrame#boxReceiptCan { background-color: #0D131F; border: 1px solid #1E293B; border-radius: 16px; }");
        ui->lblRCanCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #64748B; background: transparent;");
        ui->lblRCanPoints->setText("-");
        ui->lblRCanPoints->setStyleSheet("font-size: 15px; font-weight: 600; color: #475569; background: transparent;");
    }

    // 2. PET (페트) 영수증 렌더링
    ui->lblRPetCount->setText(QString("%1 개").arg(summary.petCount));
    if (summary.petCount > 0) {
        ui->boxReceiptPet->setStyleSheet("QFrame#boxReceiptPet { background-color: #0D1B2A; border: 1.5px solid #38BDF8; border-radius: 16px; }");
        ui->lblRPetCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #38BDF8; background: transparent;");
        ui->lblRPetPoints->setText(QString("+ %1 P").arg(summary.petCount * Config::POINT_PET));
        ui->lblRPetPoints->setStyleSheet("font-size: 15px; font-weight: 700; color: #38BDF8; background: transparent;");
    } else {
        ui->boxReceiptPet->setStyleSheet("QFrame#boxReceiptPet { background-color: #0D131F; border: 1px solid #1E293B; border-radius: 16px; }");
        ui->lblRPetCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #64748B; background: transparent;");
        ui->lblRPetPoints->setText("-");
        ui->lblRPetPoints->setStyleSheet("font-size: 15px; font-weight: 600; color: #475569; background: transparent;");
    }

    // 3. PAPER (종이) 영수증 렌더링
    ui->lblRPaperCount->setText(QString("%1 개").arg(summary.paperCount));
    if (summary.paperCount > 0) {
        ui->boxReceiptPaper->setStyleSheet("QFrame#boxReceiptPaper { background-color: #1A170F; border: 1.5px solid #F59E0B; border-radius: 16px; }");
        ui->lblRPaperCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #F59E0B; background: transparent;");
        ui->lblRPaperPoints->setText(QString("+ %1 P").arg(summary.paperCount * Config::POINT_PAPER));
        ui->lblRPaperPoints->setStyleSheet("font-size: 15px; font-weight: 700; color: #F59E0B; background: transparent;");
    } else {
        ui->boxReceiptPaper->setStyleSheet("QFrame#boxReceiptPaper { background-color: #0D131F; border: 1px solid #1E293B; border-radius: 16px; }");
        ui->lblRPaperCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #64748B; background: transparent;");
        ui->lblRPaperPoints->setText("-");
        ui->lblRPaperPoints->setStyleSheet("font-size: 15px; font-weight: 600; color: #475569; background: transparent;");
    }

    // 4. GENERAL (일반쓰레기) 영수증 렌더링
    ui->lblRGeneralCount->setText(QString("%1 개").arg(summary.generalCount));
    if (summary.generalCount > 0) {
        ui->boxReceiptGeneral->setStyleSheet("QFrame#boxReceiptGeneral { background-color: #151922; border: 1.5px solid #94A3B8; border-radius: 16px; }");
        ui->lblRGeneralCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #CBD5E1; background: transparent;");
        ui->lblRGeneralPoints->setText("0 P (일반)");
        ui->lblRGeneralPoints->setStyleSheet("font-size: 15px; font-weight: 600; color: #94A3B8; background: transparent;");
    } else {
        ui->boxReceiptGeneral->setStyleSheet("QFrame#boxReceiptGeneral { background-color: #0D131F; border: 1px solid #1E293B; border-radius: 16px; }");
        ui->lblRGeneralCount->setStyleSheet("font-size: 30px; font-weight: 900; color: #64748B; background: transparent;");
        ui->lblRGeneralPoints->setText("-");
        ui->lblRGeneralPoints->setStyleSheet("font-size: 15px; font-weight: 600; color: #475569; background: transparent;");
    }

    // 5. 총합 배너 및 회원/비회원별 안내 뱃지
    if (summary.isMember) {
        ui->lblBannerTitle->setText("총 적립 리워드");
        ui->lblTotalPoints->setText(QString("+ %1 P").arg(summary.totalPoints));
        ui->lblUserNotice->setText(QString("✅ %1 님의 계정으로 포인트가 안전하게 적립되었습니다.")
                .arg(summary.userName.isEmpty() ? "회원" : summary.userName));
        ui->lblUserNotice->setStyleSheet("background-color: rgba(16, 185, 129, 0.1); border: 1.5px solid #10B981; "
                                         "color: #10B981; border-radius: 14px; font-size: 16px; font-weight: 700; padding: 12px;");
    } else {
        ui->lblBannerTitle->setText("게스트 참여 세션");
        ui->lblTotalPoints->setText("비회원 참여");
        ui->lblUserNotice->setText("ℹ️ 모바일 앱 가입 후 스캔하시면 회차별 리워드 포인트를 적립받으실 수 있습니다.");
        ui->lblUserNotice->setStyleSheet("background-color: rgba(56, 189, 248, 0.1); border: 1.5px solid #38BDF8; "
                                         "color: #38BDF8; border-radius: 14px; font-size: 16px; font-weight: 700; padding: 12px;");
    }

    ui->lblTotalCarbon->setText(QString("🌱 총 탄소 절감량: %1g CO2").arg(QString::number(summary.totalCarbonG, 'f', 1)));

    // 6. 카운트다운 타이머 시작
    m_remainingSec = Config::RESULT_DISPLAY_TIMEOUT_SEC;
    ui->btnConfirm->setText(QString("확인 (%1초 후 처음으로 이동)").arg(m_remainingSec));
    m_countdownTimer->start();
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
    m_countdownTimer->stop();
    emit sigReturnToIdleRequested();
}