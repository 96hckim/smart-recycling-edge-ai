#include "eco_tree_controller.h"
#include "app_config.h"
#include "theme_constants.h"
#include <QLabel>
#include <QMovie>

EcoTreeController::EcoTreeController(QLabel* movieLabel, QLabel* statusLabel, QObject* parent)
    : QObject(parent)
    , m_lblMovie(movieLabel)
    , m_lblStatus(statusLabel)
{
    initMovie();
    showWaitingPlaceholder();
}

void EcoTreeController::initMovie()
{
    m_movie = new QMovie(Config::EcoTree::RESOURCE_PATH, QByteArray(), this);
    m_movie->setCacheMode(QMovie::CacheAll);
    m_movie->setScaledSize(UITheme::EcoTree::DISPLAY_SIZE);
    m_movie->setSpeed(Config::EcoTree::MOVIE_SPEED);

    if (m_lblMovie) {
        m_lblMovie->setScaledContents(false);
        m_lblMovie->setAlignment(Qt::AlignCenter);
    }

    // 목표 프레임 도달 시 정지 콜백
    connect(m_movie, &QMovie::frameChanged, this, [this](int frameNumber) {
        if (m_targetFrame > 0 && frameNumber >= m_targetFrame) {
            m_movie->setPaused(true);
        }
    });
}

TreeStage EcoTreeController::calculateStage(int count) const
{
    if (count <= 0) {
        return TreeStage::READY;
    }
    if (count <= Config::EcoTree::THRESHOLD_STAGE_1) {
        return TreeStage::SPROUT;
    }
    if (count <= Config::EcoTree::THRESHOLD_STAGE_2) {
        return TreeStage::SAPLING;
    }
    return TreeStage::MATURE;
}

void EcoTreeController::updateCount(int totalCount)
{
    const TreeStage newStage = calculateStage(totalCount);
    updateStatusText(newStage, totalCount);

    // 0개 대기 상태 복귀
    if (newStage == TreeStage::READY) {
        m_currentStage = TreeStage::READY;
        m_targetFrame = 0;
        showWaitingPlaceholder();
        return;
    }

    // 동일 단계 유지 시 애니메이션 재시작 방지
    if (newStage == m_currentStage) {
        return;
    }

    int totalFrames = m_movie->frameCount();
    if (totalFrames <= 0) {
        totalFrames = Config::EcoTree::DEFAULT_FRAME_COUNT;
    }

    // 화분 이모지에서 최초 GIF 모드로 전환되는 시점
    if (m_currentStage == TreeStage::READY) {
        m_lblMovie->setStyleSheet("");
        m_lblMovie->setMovie(m_movie);
        m_movie->jumpToFrame(0);
    }

    switch (newStage) {
    case TreeStage::SPROUT:
        m_targetFrame = static_cast<int>(totalFrames * Config::EcoTree::FRAME_RATIO_STAGE_1);
        break;
    case TreeStage::SAPLING:
        m_targetFrame = static_cast<int>(totalFrames * Config::EcoTree::FRAME_RATIO_STAGE_2);
        break;
    case TreeStage::MATURE:
        m_targetFrame = totalFrames - 1;
        break;
    case TreeStage::READY:
        m_targetFrame = 0;
        break;
    }

    m_currentStage = newStage;
    m_movie->setPaused(false);
    m_movie->start();
}

void EcoTreeController::reset()
{
    m_currentStage = TreeStage::READY;
    m_targetFrame = 0;
    showWaitingPlaceholder();
}

void EcoTreeController::showWaitingPlaceholder()
{
    if (m_movie) {
        m_movie->stop();
    }
    if (m_lblMovie) {
        m_lblMovie->setMovie(nullptr);
        m_lblMovie->setText(UITheme::EcoTree::WAITING_EMOJI);
        m_lblMovie->setStyleSheet(UITheme::EcoTree::WAITING_STYLE);
    }
    if (m_lblStatus) {
        m_lblStatus->setText(UITheme::EcoTree::STATUS_READY);
    }
}

void EcoTreeController::updateStatusText(TreeStage stage, int count)
{
    if (!m_lblStatus)
        return;

    switch (stage) {
    case TreeStage::READY:
        m_lblStatus->setText(UITheme::EcoTree::STATUS_READY);
        break;
    case TreeStage::SPROUT:
        m_lblStatus->setText(QString(UITheme::EcoTree::STATUS_STAGE_1_FMT).arg(count));
        break;
    case TreeStage::SAPLING:
        m_lblStatus->setText(QString(UITheme::EcoTree::STATUS_STAGE_2_FMT).arg(count));
        break;
    case TreeStage::MATURE:
        m_lblStatus->setText(QString(UITheme::EcoTree::STATUS_STAGE_3_FMT).arg(count));
        break;
    }
}