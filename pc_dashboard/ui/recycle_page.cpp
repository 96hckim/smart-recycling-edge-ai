#include "recycle_page.h"
#include "ui_recycle_page.h"

RecyclePage::RecyclePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RecyclePage)
{
    ui->setupUi(this);
}

RecyclePage::~RecyclePage()
{
    delete ui;
}

void RecyclePage::startSession(bool isMember, const QString& userName)
{
    m_isMember = isMember;
    m_userName = userName;
    resetState();

    if (m_isMember) {
        ui->lblUserGreeting->setText(QString("👤 %1 님 환영합니다").arg(m_userName.isEmpty() ? "회원" : m_userName));
        ui->lblUserGreeting->setStyleSheet("font-size: 24px; font-weight: 800; color: #10B981; background: transparent;");
        ui->lblSessionMode->setText("투입 완료 후 포인트가 자동으로 적립됩니다.");
        ui->lblRewardHeader->setText("💰 현재 세션 획득 리워드");
    } else {
        ui->lblUserGreeting->setText("👤 비회원 간편 투입");
        ui->lblUserGreeting->setStyleSheet("font-size: 24px; font-weight: 800; color: #38BDF8; background: transparent;");
        ui->lblSessionMode->setText("비회원 모드로 동작 중입니다 (포인트 미적립).");
        ui->lblRewardHeader->setText("🌱 절감 환경 리워드");
    }
}

void RecyclePage::updateFrame(const QPixmap& pixmap)
{
    if (!pixmap.isNull()) {
        ui->lblVideo->setPixmap(pixmap.scaled(ui->lblVideo->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    }
}

void RecyclePage::updateDetectionState(const QString& className, double confidence, int debounceCount)
{
    if (className.isEmpty()) {
        ui->lblDetectClass->setText("물품 인식 대기 중...");
        ui->lblDetectConfidence->setText("신뢰도: - %");
        ui->progressBarDebounce->setValue(0);
        ui->lblGuideBanner->setText("🎯 카메라 중앙 영역에 재활용품을 놓아주세요");
        ui->lblGuideBanner->setStyleSheet("background-color: #111827; border: 1px solid #1F2937; border-radius: 14px; color: #38BDF8; font-size: 20px; font-weight: 700; padding: 8px;");
        return;
    }

    ui->lblDetectClass->setText(QString("🔍 %1 감지됨").arg(className.toUpper()));
    ui->lblDetectConfidence->setText(QString("신뢰도: %1%").arg(QString::number(confidence * 100.0, 'f', 1)));
    ui->progressBarDebounce->setValue(debounceCount);

    if (debounceCount >= Config::STABLE_FRAME_THRESHOLD) {
        if (className.toUpper().contains("GENERAL") || className.contains("일반")) {
            ui->lblGuideBanner->setText("⚠️ 일반쓰레기 감지 (포인트 미지급)");
            ui->lblGuideBanner->setStyleSheet("background-color: rgba(148, 163, 184, 0.2); border: 1.5px solid #94A3B8; border-radius: 14px; color: #F1F5F9; font-size: 20px; font-weight: 800; padding: 8px;");
        } else {
            ui->lblGuideBanner->setText(QString("✅ %1 인식 확정! 투입구에 넣어주세요").arg(className.toUpper()));
            ui->lblGuideBanner->setStyleSheet("background-color: rgba(16, 185, 129, 0.15); border: 1.5px solid #10B981; border-radius: 14px; color: #10B981; font-size: 20px; font-weight: 800; padding: 8px;");
        }
    }
}

void RecyclePage::updateSessionSummary(int canCount, int petCount, int paperCount, int generalCount, int totalPoints, double totalCarbon)
{
    ui->lblCanCount->setText(QString::number(canCount));
    ui->lblPetCount->setText(QString::number(petCount));
    ui->lblPaperCount->setText(QString::number(paperCount));
    ui->lblGeneralCount->setText(QString::number(generalCount));

    if (m_isMember) {
        ui->lblTotalPoints->setText(QString("+ %1 P").arg(totalPoints));
    } else {
        ui->lblTotalPoints->setText("비회원 (0 P)");
    }
    ui->lblTotalCarbon->setText(QString("🌱 절감 탄소량: %1g CO2").arg(QString::number(totalCarbon, 'f', 1)));
}

void RecyclePage::resetState()
{
    ui->lblVideo->clear();
    ui->lblVideo->setText("Jetson AI 비전 스트림 연결 중...");
    ui->lblCanCount->setText("0");
    ui->lblPetCount->setText("0");
    ui->lblPaperCount->setText("0");
    ui->lblGeneralCount->setText("0");
    ui->lblTotalPoints->setText("+ 0 P");
    ui->lblTotalCarbon->setText("🌱 절감 탄소량: 0.0g CO2");
    ui->lblDetectClass->setText("물품 인식 대기 중...");
    ui->lblDetectConfidence->setText("신뢰도: - %");
    ui->progressBarDebounce->setValue(0);
}

void RecyclePage::on_btnFinishSession_clicked()
{
    emit sigFinishSessionRequested();
}

void RecyclePage::on_btnCancelSession_clicked()
{
    emit sigCancelSessionRequested();
}