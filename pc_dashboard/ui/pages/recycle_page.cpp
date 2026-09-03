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
        const QString displayName = m_userName.isEmpty() ? UITheme::Recycle::Text::DEFAULT_MEMBER_NAME : m_userName;
        ui->lblUserGreeting->setText(QString(UITheme::Recycle::Text::GREETING_MEMBER_FMT).arg(displayName));
        ui->lblUserGreeting->setStyleSheet(UITheme::Recycle::GREETING_MEMBER);
        ui->lblSessionMode->setText(UITheme::Recycle::Text::SESSION_MODE_MEMBER);
        ui->lblRewardHeader->setText(UITheme::Recycle::Text::REWARD_HEADER_MEMBER);
    } else {
        ui->lblUserGreeting->setText(UITheme::Recycle::Text::GREETING_GUEST);
        ui->lblUserGreeting->setStyleSheet(UITheme::Recycle::GREETING_GUEST);
        ui->lblSessionMode->setText(UITheme::Recycle::Text::SESSION_MODE_GUEST);
        ui->lblRewardHeader->setText(UITheme::Recycle::Text::REWARD_HEADER_GUEST);
    }
}

void RecyclePage::updateFrame(const QPixmap& pixmap)
{
    if (pixmap.isNull())
        return;

    const QSize targetSize = ui->lblVideo->size();
    if (targetSize.width() <= 0 || targetSize.height() <= 0)
        return;

    // 1. 원본 영상을 화면(lblVideo) 크기 비율에 맞춰 스케일링
    QPixmap frame = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 2. 확대된 화면 위에 직접 바운딩 박스 및 배지 렌더링
    if (!m_detectionBox.isNull() && pixmap.width() > 0 && pixmap.height() > 0) {
        const double scaleX = static_cast<double>(frame.width()) / pixmap.width();
        const double scaleY = static_cast<double>(frame.height()) / pixmap.height();

        const QRect scaledBox(
            static_cast<int>(m_detectionBox.x() * scaleX),
            static_cast<int>(m_detectionBox.y() * scaleY),
            static_cast<int>(m_detectionBox.width() * scaleX),
            static_cast<int>(m_detectionBox.height() * scaleY));

        QPainter painter(&frame);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        // 1) 테마 색상 테두리
        painter.setPen(QPen(m_boxColor, UITheme::Recycle::BOX_PEN_WIDTH));
        painter.drawRect(scaledBox);

        // 2) 박스 상단 한글 배지
        if (!m_boxLabel.isEmpty()) {
            QFont font(UITheme::FONT_FAMILY, UITheme::Recycle::BADGE_FONT_SIZE, QFont::Bold);
            font.setStyleHint(QFont::SansSerif);
            painter.setFont(font);
            QFontMetrics fm(font);

            const int padX = UITheme::Recycle::BADGE_PAD_X;
            const int padY = UITheme::Recycle::BADGE_PAD_Y;
            const int badgeW = fm.horizontalAdvance(m_boxLabel) + (padX * 2);
            const int badgeH = fm.height() + (padY * 2);

            int badgeY = scaledBox.top() - badgeH;
            if (badgeY < 0) {
                badgeY = scaledBox.top();
            }
            int badgeX = std::max(0, scaledBox.left());

            const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

            painter.fillRect(badgeRect, m_boxColor);
            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, m_boxLabel);
        }
    }

    // 3. 표출
    ui->lblVideo->setPixmap(frame);
}

void RecyclePage::updateDetectionState(const QString& className, double confidence, int debounceCount, const QRect& box)
{
    if (className.isEmpty() || confidence < Config::MIN_CONFIDENCE_THRESHOLD) {
        m_detectionBox = QRect();
        m_boxLabel.clear();
        setGuideBanner(UITheme::Recycle::BannerType::READY);
        return;
    }

    const RecycleCategory cat = Config::parseCategory(className);
    const QString displayCategoryName = Config::getCategoryNameKo(cat);

    m_detectionBox = box;
    m_boxColor = UITheme::getCategoryColor(cat);
    m_boxLabel = displayCategoryName;

    if (debounceCount >= Config::STABLE_FRAME_THRESHOLD) {
        if (cat != RecycleCategory::UNKNOWN) {
            setGuideBanner(UITheme::Recycle::BannerType::CONFIRMED, displayCategoryName);
        } else {
            setGuideBanner(UITheme::Recycle::BannerType::WARNING);
        }
    } else {
        setGuideBanner(UITheme::Recycle::BannerType::ANALYZING, displayCategoryName);
    }
}

void RecyclePage::updateSessionSummary(const SessionSummary& summary)
{
    // 순서: 종이 -> 캔 -> 페트 -> 비닐
    ui->lblPaperCount->setText(QString::number(summary.paperCount));
    ui->lblCanCount->setText(QString::number(summary.canCount));
    ui->lblPetCount->setText(QString::number(summary.petCount));
    ui->lblVinylCount->setText(QString::number(summary.vinylCount));

    // 4개 품목 모두 정식 투입 유효 아이템
    const int validItemCount = summary.paperCount + summary.canCount + summary.petCount + summary.vinylCount;
    ui->btnFinishSession->setEnabled(validItemCount > 0);

    if (summary.isMember) {
        ui->lblTotalPoints->setText(QString(UITheme::Recycle::Text::POINTS_MEMBER_FMT).arg(summary.totalPoints));
        ui->lblTotalPoints->setStyleSheet(UITheme::Recycle::POINTS_MEMBER);
    } else {
        ui->lblTotalPoints->setText(QString(UITheme::Recycle::Text::POINTS_GUEST_FMT).arg(summary.totalPoints));
        ui->lblTotalPoints->setStyleSheet(UITheme::Recycle::POINTS_GUEST);
    }

    ui->lblTotalCarbon->setText(QString(UITheme::Recycle::Text::CARBON_SAVED_FMT)
            .arg(QString::number(summary.totalCarbonG, 'f', 1)));

    if (m_ecoTree) {
        m_ecoTree->updateCount(validItemCount);
    }
}

void RecyclePage::resetState()
{
    m_detectionBox = QRect();
    m_boxLabel.clear();

    ui->lblVideo->clear();
    ui->lblVideo->setText(UITheme::Recycle::Text::VIDEO_INITIALIZING);

    // 순서: 종이 -> 캔 -> 페트 -> 비닐
    ui->lblPaperCount->setText(QString::number(0));
    ui->lblCanCount->setText(QString::number(0));
    ui->lblPetCount->setText(QString::number(0));
    ui->lblVinylCount->setText(QString::number(0));
    ui->btnFinishSession->setEnabled(false);

    if (m_isMember) {
        ui->lblTotalPoints->setText(QString(UITheme::Recycle::Text::POINTS_MEMBER_FMT).arg(0));
        ui->lblTotalPoints->setStyleSheet(UITheme::Recycle::POINTS_MEMBER);
    } else {
        ui->lblTotalPoints->setText(QString(UITheme::Recycle::Text::POINTS_GUEST_FMT).arg(0));
        ui->lblTotalPoints->setStyleSheet(UITheme::Recycle::POINTS_GUEST);
    }

    ui->lblTotalCarbon->setText(QString(UITheme::Recycle::Text::CARBON_SAVED_FMT).arg(QString::number(0.0, 'f', 1)));
    setGuideBanner(UITheme::Recycle::BannerType::READY);

    if (m_ecoTree) {
        m_ecoTree->reset();
    }
}

void RecyclePage::setGuideBanner(UITheme::Recycle::BannerType type, const QString& customText)
{
    const auto theme = UITheme::Recycle::getBannerTheme(type);

    QString message;
    switch (type) {
    case UITheme::Recycle::BannerType::READY:
        message = UITheme::Recycle::Text::GUIDE_READY;
        break;
    case UITheme::Recycle::BannerType::ANALYZING:
        message = QString(UITheme::Recycle::Text::GUIDE_ANALYZING_FMT).arg(customText);
        break;
    case UITheme::Recycle::BannerType::CONFIRMED:
        message = QString(UITheme::Recycle::Text::GUIDE_CONFIRMED_FMT).arg(customText);
        break;
    case UITheme::Recycle::BannerType::WARNING:
        message = UITheme::Recycle::Text::GUIDE_GENERAL_WARN;
        break;
    }

    if (ui->lblGuideBanner->text() == message)
        return;

    ui->lblGuideBanner->setText(message);
    ui->lblGuideBanner->setStyleSheet(
        QString(UITheme::Recycle::BANNER_TEMPLATE).arg(theme.bgColor, theme.borderColor, theme.textColor));
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