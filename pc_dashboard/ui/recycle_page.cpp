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
        const QString displayName = m_userName.isEmpty() ? "회원" : m_userName;
        ui->lblUserGreeting->setText(QString("👤 %1 님 환영합니다").arg(displayName));
        ui->lblUserGreeting->setStyleSheet("font-size: 30px; font-weight: 900; color: #10B981; background: transparent;");
        ui->lblSessionMode->setText("투입 완료 후 포인트가 자동으로 적립됩니다.");
        ui->lblRewardHeader->setText("💰 현재 세션 획득 리워드");
    } else {
        ui->lblUserGreeting->setText("👤 비회원 간편 투입");
        ui->lblUserGreeting->setStyleSheet("font-size: 30px; font-weight: 900; color: #38BDF8; background: transparent;");
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
    // 유효 신뢰도 미달이거나 물품 미인식 시 대기 상태 전환
    if (className.isEmpty() || confidence < Config::MIN_CONFIDENCE_THRESHOLD) {
        ui->lblDetectClass->setText("물품 인식 대기 중...");
        ui->lblDetectConfidence->setText("신뢰도: - %");
        ui->progressBarDebounce->setValue(0);
        setGuideBanner("🎯 카메라 중앙 영역에 재활용품을 놓아주세요", "#38BDF8", "#0284C7", "#0D1927");
        return;
    }

    // Config 파서로 품목 식별
    const RecycleCategory cat = Config::parseCategory(className);
    const QString upperClass = className.toUpper();

    ui->lblDetectClass->setText(QString("🔍 %1 감지됨").arg(upperClass));
    ui->lblDetectConfidence->setText(QString("신뢰도: %1%").arg(QString::number(confidence * 100.0, 'f', 1)));
    ui->progressBarDebounce->setValue(debounceCount);

    if (debounceCount >= Config::STABLE_FRAME_THRESHOLD) {
        if (cat == RecycleCategory::GENERAL) {
            setGuideBanner("⚠️ 일반쓰레기 감지 (포인트 미지급)", "#F1F5F9", "#94A3B8", "rgba(148, 163, 184, 0.2)");
        } else if (cat != RecycleCategory::UNKNOWN) {
            setGuideBanner(QString("✅ %1 인식 확정! 투입구에 넣어주세요").arg(upperClass),
                "#10B981", "#10B981", "rgba(16, 185, 129, 0.15)");
        }
    }
}

void RecyclePage::updateSessionSummary(const SessionSummary& summary)
{
    ui->lblCanCount->setText(QString::number(summary.canCount));
    ui->lblPetCount->setText(QString::number(summary.petCount));
    ui->lblPaperCount->setText(QString::number(summary.paperCount));
    ui->lblGeneralCount->setText(QString::number(summary.generalCount));

    if (summary.isMember) {
        ui->lblTotalPoints->setText(QString("+ %1 P").arg(summary.totalPoints));
        ui->lblTotalPoints->setStyleSheet("font-size: 42px; font-weight: 900; color: #38BDF8; background: transparent;");
    } else {
        ui->lblTotalPoints->setText("0 P (비회원)");
        ui->lblTotalPoints->setStyleSheet("font-size: 34px; font-weight: 800; color: #64748B; background: transparent;");
    }

    ui->lblTotalCarbon->setText(QString("🌱 절감 탄소량: %1g CO2").arg(QString::number(summary.totalCarbonG, 'f', 1)));
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
    setGuideBanner("🎯 카메라 중앙 영역에 재활용품을 놓아주세요", "#38BDF8", "#0284C7", "#0D1927");
}

void RecyclePage::setGuideBanner(const QString& text, const QString& textColor,
    const QString& borderColor, const QString& bgColor)
{
    ui->lblGuideBanner->setText(text);
    ui->lblGuideBanner->setStyleSheet(
        QString("background-color: %1; border: 2px solid %2; border-radius: 16px; color: %3; font-size: 22px; font-weight: 800; padding: 10px;")
            .arg(bgColor, borderColor, textColor));
}

void RecyclePage::on_btnFinishSession_clicked()
{
    emit sigFinishSessionRequested();
}

void RecyclePage::on_btnCancelSession_clicked()
{
    emit sigCancelSessionRequested();
}