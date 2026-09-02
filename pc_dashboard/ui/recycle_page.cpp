#include "recycle_page.h"
#include "eco_tree_controller.h"
#include "ui_recycle_page.h"
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <algorithm>

RecyclePage::RecyclePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RecyclePage)
{
    ui->setupUi(this);
    ui->lblVideo->setAlignment(Qt::AlignCenter);
    ui->lblVideo->setScaledContents(false);

    m_ecoTree = new EcoTreeController(ui->lblEcoTree, ui->lblEcoTreeStatus, this);
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

    applyDynamicProperty(ui->lblUserGreeting, UITheme::PROP_MEMBER, m_isMember);
    applyDynamicProperty(ui->lblTotalPoints, UITheme::PROP_MEMBER, m_isMember);

    if (m_isMember) {
        // ★ 하드코딩 "회원" -> UITheme::Text::DEFAULT_MEMBER_NAME 상수로 대체
        const QString displayName = m_userName.isEmpty() ? UITheme::Text::DEFAULT_MEMBER_NAME : m_userName;
        ui->lblUserGreeting->setText(QString(UITheme::Text::GREETING_MEMBER_FMT).arg(displayName));
        ui->lblUserGreeting->setStyleSheet(UITheme::Style::GREETING_MEMBER);
        ui->lblSessionMode->setText(UITheme::Text::SESSION_MODE_MEMBER);
        ui->lblRewardHeader->setText(UITheme::Text::REWARD_HEADER_MEMBER);
    } else {
        ui->lblUserGreeting->setText(UITheme::Text::GREETING_GUEST);
        ui->lblUserGreeting->setStyleSheet(UITheme::Style::GREETING_GUEST);
        ui->lblSessionMode->setText(UITheme::Text::SESSION_MODE_GUEST);
        ui->lblRewardHeader->setText(UITheme::Text::REWARD_HEADER_GUEST);
    }
}

void RecyclePage::updateFrame(const QPixmap& pixmap)
{
    if (pixmap.isNull())
        return;

    const QSize targetSize = ui->lblVideo->size();
    if (targetSize.width() <= 0 || targetSize.height() <= 0)
        return;

    QPixmap frame = pixmap;

    if (!m_detectionBox.isNull()) {
        QPainter painter(&frame);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        // 1) 품목 테마 색상 테두리 (상수 두께 적용)
        painter.setPen(QPen(m_boxColor, Config::VisionRender::BOX_PEN_WIDTH));
        painter.drawRect(m_detectionBox);

        // 2) 박스 상단 한글 품목 배지 ("캔", "페트", "종이", "일반")
        if (!m_boxLabel.isEmpty()) {
            QFont font(UITheme::Style::FONT_FAMILY, Config::VisionRender::BADGE_FONT_SIZE, QFont::Bold);
            font.setStyleHint(QFont::SansSerif);
            painter.setFont(font);
            QFontMetrics fm(font);

            const int padX = Config::VisionRender::BADGE_PAD_X;
            const int padY = Config::VisionRender::BADGE_PAD_Y;
            const int badgeW = fm.horizontalAdvance(m_boxLabel) + (padX * 2);
            const int badgeH = fm.height() + (padY * 2);

            int badgeY = m_detectionBox.top() - badgeH;
            if (badgeY < 0) {
                badgeY = m_detectionBox.top();
            }
            int badgeX = std::max(0, m_detectionBox.left());

            const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

            painter.fillRect(badgeRect, m_boxColor);
            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, m_boxLabel);
        }
    }

    ui->lblVideo->setPixmap(frame.scaled(targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
}

void RecyclePage::updateDetectionState(const QString& className, double confidence, int debounceCount, const QRect& box)
{
    if (className.isEmpty() || confidence < Config::MIN_CONFIDENCE_THRESHOLD) {
        m_detectionBox = QRect();
        m_boxLabel.clear();
        setGuideBanner(UITheme::BannerType::READY);
        return;
    }

    const RecycleCategory cat = Config::parseCategory(className);
    const QString displayCategoryName = Config::getCategoryNameKo(cat);

    m_detectionBox = box;
    m_boxColor = Config::getCategoryColor(cat);
    m_boxLabel = displayCategoryName;

    if (debounceCount >= Config::STABLE_FRAME_THRESHOLD) {
        if (cat == RecycleCategory::GENERAL) {
            setGuideBanner(UITheme::BannerType::WARNING);
        } else if (cat != RecycleCategory::UNKNOWN) {
            setGuideBanner(UITheme::BannerType::CONFIRMED, displayCategoryName);
        }
    } else {
        setGuideBanner(UITheme::BannerType::ANALYZING, displayCategoryName);
    }
}

void RecyclePage::updateSessionSummary(const SessionSummary& summary)
{
    ui->lblCanCount->setText(QString::number(summary.canCount));
    ui->lblPetCount->setText(QString::number(summary.petCount));
    ui->lblPaperCount->setText(QString::number(summary.paperCount));
    ui->lblGeneralCount->setText(QString::number(summary.generalCount));

    if (summary.isMember) {
        ui->lblTotalPoints->setText(QString(UITheme::Text::POINTS_MEMBER_FMT).arg(summary.totalPoints));
        ui->lblTotalPoints->setStyleSheet(UITheme::Style::POINTS_MEMBER);
    } else {
        ui->lblTotalPoints->setText(QString(UITheme::Text::POINTS_GUEST_FMT).arg(summary.totalPoints));
        ui->lblTotalPoints->setStyleSheet(UITheme::Style::POINTS_GUEST);
    }

    ui->lblTotalCarbon->setText(QString(UITheme::Text::CARBON_SAVED_FMT)
            .arg(QString::number(summary.totalCarbonG, 'f', 1)));

    if (m_ecoTree) {
        m_ecoTree->updateCount(summary.canCount + summary.petCount + summary.paperCount);
    }
}

void RecyclePage::resetState()
{
    m_detectionBox = QRect();
    m_boxLabel.clear();

    ui->lblVideo->clear();
    ui->lblVideo->setText(UITheme::Text::VIDEO_INITIALIZING);
    ui->lblCanCount->setText(QString::number(0));
    ui->lblPetCount->setText(QString::number(0));
    ui->lblPaperCount->setText(QString::number(0));
    ui->lblGeneralCount->setText(QString::number(0));

    if (m_isMember) {
        ui->lblTotalPoints->setText(QString(UITheme::Text::POINTS_MEMBER_FMT).arg(0));
        ui->lblTotalPoints->setStyleSheet(UITheme::Style::POINTS_MEMBER);
    } else {
        ui->lblTotalPoints->setText(QString(UITheme::Text::POINTS_GUEST_FMT).arg(0));
        ui->lblTotalPoints->setStyleSheet(UITheme::Style::POINTS_GUEST);
    }

    ui->lblTotalCarbon->setText(QString(UITheme::Text::CARBON_SAVED_FMT).arg(QString::number(0.0, 'f', 1)));
    setGuideBanner(UITheme::BannerType::READY);

    if (m_ecoTree) {
        m_ecoTree->reset();
    }
}

void RecyclePage::setGuideBanner(UITheme::BannerType type, const QString& customText)
{
    const auto theme = UITheme::getBannerTheme(type);

    QString message;
    switch (type) {
    case UITheme::BannerType::READY:
        message = UITheme::Text::GUIDE_READY;
        break;
    case UITheme::BannerType::ANALYZING:
        message = QString(UITheme::Text::GUIDE_ANALYZING_FMT).arg(customText);
        break;
    case UITheme::BannerType::CONFIRMED:
        message = QString(UITheme::Text::GUIDE_CONFIRMED_FMT).arg(customText);
        break;
    case UITheme::BannerType::WARNING:
        message = UITheme::Text::GUIDE_GENERAL_WARN;
        break;
    }

    if (ui->lblGuideBanner->text() == message)
        return;

    ui->lblGuideBanner->setText(message);
    ui->lblGuideBanner->setStyleSheet(
        QString(UITheme::Style::BANNER_TEMPLATE).arg(theme.bgColor, theme.borderColor, theme.textColor));
}

void RecyclePage::applyDynamicProperty(QWidget* widget, const char* propName, const QVariant& value)
{
    if (!widget)
        return;
    widget->setProperty(propName, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void RecyclePage::on_btnFinishSession_clicked()
{
    emit sigFinishSessionRequested();
}

void RecyclePage::on_btnCancelSession_clicked()
{
    emit sigCancelSessionRequested();
}