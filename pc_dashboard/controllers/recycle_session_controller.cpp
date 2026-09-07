#include "recycle_session_controller.h"
#include "theme_constants.h"
#include <QDebug>

RecycleSessionController::RecycleSessionController(QObject *parent)
    : QObject(parent)
{
}

void RecycleSessionController::startSession(bool isMember, const QString &userName, int userId)
{
    m_isActive = true;
    m_summary.reset();
    m_summary.isMember = isMember;
    m_summary.userName = userName;
    m_userId = userId;

    m_consecutiveDetections = 0;
    m_lastCategory = RecycleCategory::UNKNOWN;
    m_itemCounted = false;

    emit sigSessionUpdated(m_summary);
    emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::READY), QString());
    emit sigDetectionBoxUpdated("", 0.0, 0, QRect());
}

void RecycleSessionController::finishSession()
{
    m_isActive = false;
    m_consecutiveDetections = 0;
    m_itemCounted = false;
    m_userId = -1;
}

void RecycleSessionController::cancelSession()
{
    m_isActive = false;
    m_summary.reset();
    m_consecutiveDetections = 0;
    m_itemCounted = false;
    m_userId = -1;
}

void RecycleSessionController::processFrameMetadata(const FrameMetadata &meta)
{
    if (!m_isActive)
    {
        return;
    }

    const bool hasDetection = !meta.detections.isEmpty();
    const Detection top = hasDetection ? meta.detections.first() : Detection();

    // 1. 도어 열림 (OPEN): 비전 카운팅 완전 중단
    if (meta.door.isOpen)
    {
        m_consecutiveDetections = 0;
        emit sigDetectionBoxUpdated(top.className, top.confidence, 0, top.box);
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::DOOR_OPEN),
                                     Config::getCategoryNameKo(m_lastCategory));
        return;
    }

    // 2. 물체 없음: 대기 상태로 리셋
    if (!hasDetection || top.category == RecycleCategory::UNKNOWN)
    {
        m_consecutiveDetections = 0;
        m_lastCategory = RecycleCategory::UNKNOWN;
        m_itemCounted = false;

        emit sigDetectionBoxUpdated("", 0.0, 0, QRect());
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::READY), QString());
        return;
    }

    // 3. 물체 변경 시 디바운스 리셋
    if (top.category != m_lastCategory)
    {
        m_lastCategory = top.category;
        m_consecutiveDetections = 0;
        m_itemCounted = false;
    }

    m_consecutiveDetections++;
    emit sigDetectionBoxUpdated(top.className, top.confidence, m_consecutiveDetections, top.box);

    // 4. 18프레임 연속 인식 시 1회 카운트 (+1)
    if (m_consecutiveDetections >= Config::STABLE_FRAME_THRESHOLD)
    {
        if (!m_itemCounted)
        {
            m_itemCounted = true;
            m_summary.addItem(top.category, 1);
            emit sigSessionUpdated(m_summary);
            emit sigItemCounted(top.category, m_summary);

            qDebug() << "[Session] 품목 인식 확정 ->" << Config::getCategoryNameKo(top.category)
                     << "(총" << m_summary.totalPoints << "P)";
        }
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::CONFIRMED),
                                     Config::getCategoryNameKo(top.category));
    }
    else
    {
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::ANALYZING),
                                     Config::getCategoryNameKo(top.category));
    }
}
