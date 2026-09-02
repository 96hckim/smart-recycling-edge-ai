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
    showBaseTree();
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
        m_lblMovie->setStyleSheet("background: transparent;");
        m_lblMovie->setMovie(m_movie);
    }

    // 목표 프레임 도달 시 정지
    connect(m_movie, &QMovie::frameChanged, this, [this](int frameNumber) {
        if (m_targetFrame > 0 && frameNumber >= m_targetFrame) {
            m_movie->setPaused(true);
        }
    });
}

TreeStage EcoTreeController::calculateStage(int count) const
{
    if (count <= 0) {
        return TreeStage::BASE_TREE;
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

    // 0개로 리셋되는 경우 잎 없는 기본 나무 프레임으로 복귀
    if (newStage == TreeStage::BASE_TREE) {
        reset();
        return;
    }

    // 동일 단계 유지 시 중복 재생 방지
    if (newStage == m_currentStage) {
        return;
    }

    int totalFrames = m_movie->frameCount();
    if (totalFrames <= 0) {
        totalFrames = Config::EcoTree::DEFAULT_FRAME_COUNT;
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
    case TreeStage::BASE_TREE:
        m_targetFrame = static_cast<int>(totalFrames * Config::EcoTree::FRAME_RATIO_BASE);
        break;
    }

    m_currentStage = newStage;
    m_movie->setPaused(false);
    m_movie->start();
}

void EcoTreeController::reset()
{
    m_currentStage = TreeStage::BASE_TREE;
    showBaseTree();
}

void EcoTreeController::showBaseTree()
{
    if (!m_movie) {
        return;
    }

    m_movie->stop();

    int totalFrames = m_movie->frameCount();
    if (totalFrames <= 0) {
        totalFrames = Config::EcoTree::DEFAULT_FRAME_COUNT;
    }

    // 1단계(잎 없는 나무) 해당 프레임 위치로 이동 후 일시정지 상태 유지
    m_targetFrame = static_cast<int>(totalFrames * Config::EcoTree::FRAME_RATIO_BASE);
    m_movie->jumpToFrame(m_targetFrame);

    if (m_lblStatus) {
        m_lblStatus->setText(UITheme::EcoTree::STATUS_BASE);
    }
}

void EcoTreeController::updateStatusText(TreeStage stage, int count)
{
    if (!m_lblStatus)
        return;

    switch (stage) {
    case TreeStage::BASE_TREE:
        m_lblStatus->setText(UITheme::EcoTree::STATUS_BASE);
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