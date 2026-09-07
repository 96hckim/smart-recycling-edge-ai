/**
 * 연속 인식 디바운스 및 하드웨어 투입 연동 비전 카운팅 FSM 구현부.
 */
#include "recycle_session_controller.h"
#include "theme_constants.h"
#include <QDebug>

RecycleSessionController::RecycleSessionController(QObject* parent)
    : QObject(parent)
{
}

void RecycleSessionController::startSession(bool isMember, const QString& userName, int userId)
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

void RecycleSessionController::processFrameMetadata(const FrameMetadata& meta)
{
    if (!m_isActive) {
        return;
    }

    const bool hasDetection = !meta.detections.isEmpty();
    const Detection top = hasDetection ? meta.detections.first() : Detection();

    // 1. 하드웨어 도어 개방 중: 투입 진행 중인 물체의 중복 카운트 방지를 위해 비전 카운팅 중단 (Interlock)
    if (meta.door.isOpen) {
        m_consecutiveDetections = 0;
        emit sigDetectionBoxUpdated(top.className, top.confidence, 0, top.box);

        const RecycleCategory doorCat = Config::parseCategory(meta.door.item);
        QString doorItemName;
        if (doorCat != RecycleCategory::UNKNOWN) {
            doorItemName = Config::getCategoryNameKo(doorCat);
        } else if (m_lastCategory != RecycleCategory::UNKNOWN) {
            doorItemName = Config::getCategoryNameKo(m_lastCategory);
        } else {
            doorItemName = (meta.door.item.isEmpty() || meta.door.item == "ALL") ? "투입구" : meta.door.item;
        }

        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::DOOR_OPEN), doorItemName);
        return;
    }

    // 2. 검출 객체 부재 또는 미분류: 대기 상태 복귀
    if (!hasDetection || top.category == RecycleCategory::UNKNOWN) {
        m_consecutiveDetections = 0;
        m_lastCategory = RecycleCategory::UNKNOWN;
        m_itemCounted = false;

        emit sigDetectionBoxUpdated("", 0.0, 0, QRect());
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::READY), QString());
        return;
    }

    // 3. 검출 품목 변경 감지: 이전 디바운스 카운트 초기화
    if (top.category != m_lastCategory) {
        m_lastCategory = top.category;
        m_consecutiveDetections = 0;
        m_itemCounted = false;
    }

    m_consecutiveDetections++;
    emit sigDetectionBoxUpdated(top.className, top.confidence, m_consecutiveDetections, top.box);

    // 4. 안정 프레임(18회) 연속 감지 시 단일 객체 투입으로 확정 처리 (중복 가산 방지 플래그 적용)
    if (m_consecutiveDetections >= Config::STABLE_FRAME_THRESHOLD) {
        if (!m_itemCounted) {
            m_itemCounted = true;
            m_summary.addItem(top.category, 1);
            emit sigSessionUpdated(m_summary);
            emit sigItemCounted(top.category, m_summary);

            qDebug() << "[Session] 품목 인식 확정 ->" << Config::getCategoryNameKo(top.category)
                     << "(총" << m_summary.totalPoints << "P)";
        }
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::CONFIRMED),
            Config::getCategoryNameKo(top.category));
    } else {
        emit sigGuideBannerRequested(static_cast<int>(UITheme::Recycle::BannerType::ANALYZING),
            Config::getCategoryNameKo(top.category));
    }
}