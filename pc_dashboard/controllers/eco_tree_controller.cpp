/**
 * QMovie 특정 프레임 타겟팅 및 단계별 정지 기반 동적 애니메이션 제어 구현부.
 */
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
    m_movie = new QMovie(UITheme::EcoTree::RESOURCE_PATH, QByteArray(), this);
    m_movie->setCacheMode(QMovie::CacheAll);
    m_movie->setScaledSize(UITheme::EcoTree::DISPLAY_SIZE);
    m_movie->setSpeed(UITheme::EcoTree::MOVIE_SPEED);

    if (m_lblMovie) {
        m_lblMovie->setScaledContents(false);
        m_lblMovie->setAlignment(Qt::AlignCenter);
        m_lblMovie->setStyleSheet("background: transparent;");
        m_lblMovie->setMovie(m_movie);
    }

    // 계산된 단계별 목표 프레임 인덱스 도달 시 일시정지 (무한 루프 방지)
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

    if (newStage == TreeStage::BASE_TREE) {
        reset();
        return;
    }

    // 동일 단계 내 추가 투입 시 불필요한 애니메이션 재시작 배제
    if (newStage == m_currentStage) {
        return;
    }

    int totalFrames = m_movie->frameCount();
    if (totalFrames <= 0) {
        totalFrames = UITheme::EcoTree::DEFAULT_FRAME_COUNT;
    }

    // GIF 전체 프레임 중 각 성장 단계에 해당하는 목표 지점(Ratio) 산출
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
        totalFrames = UITheme::EcoTree::DEFAULT_FRAME_COUNT;
    }

    // 초기 상태(잎 없는 기본 나무) 프레임 위치로 이동 후 렌더링 유지
    m_targetFrame = static_cast<int>(totalFrames * Config::EcoTree::FRAME_RATIO_BASE);
    m_movie->jumpToFrame(m_targetFrame);

    if (m_lblStatus) {
        m_lblStatus->setText(UITheme::EcoTree::Text::STATUS_BASE);
    }
}

void EcoTreeController::updateStatusText(TreeStage stage, int count)
{
    if (!m_lblStatus)
        return;

    switch (stage) {
    case TreeStage::BASE_TREE:
        m_lblStatus->setText(UITheme::EcoTree::Text::STATUS_BASE);
        break;
    case TreeStage::SPROUT:
        m_lblStatus->setText(QString(UITheme::EcoTree::Text::STATUS_STAGE_1_FMT).arg(count));
        break;
    case TreeStage::SAPLING:
        m_lblStatus->setText(QString(UITheme::EcoTree::Text::STATUS_STAGE_2_FMT).arg(count));
        break;
    case TreeStage::MATURE:
        m_lblStatus->setText(QString(UITheme::EcoTree::Text::STATUS_STAGE_3_FMT).arg(count));
        break;
    }
}