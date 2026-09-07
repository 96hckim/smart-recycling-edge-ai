#pragma once
#ifndef RECYCLE_SESSION_CONTROLLER_H
#define RECYCLE_SESSION_CONTROLLER_H

#include "app_config.h"
#include <QObject>
#include <QRect>
#include <QString>

class RecycleSessionController : public QObject
{
    Q_OBJECT

public:
    explicit RecycleSessionController(QObject *parent = nullptr);
    ~RecycleSessionController() override = default;

    void startSession(bool isMember, const QString &userName = QString(), int userId = -1);
    void finishSession();
    void cancelSession();
    bool isSessionActive() const { return m_isActive; }

    void processFrameMetadata(const FrameMetadata &meta);

    const SessionSummary &sessionSummary() const { return m_summary; }
    int currentUserId() const { return m_userId; }

signals:
    void sigSessionUpdated(const SessionSummary &summary);
    void sigGuideBannerRequested(int bannerType, const QString &customText);
    void sigDetectionBoxUpdated(const QString &className, double confidence, int debounceCount, const QRect &box);
    void sigItemCounted(RecycleCategory category, const SessionSummary &summary);

private:
    bool m_isActive{false};
    SessionSummary m_summary;
    int m_userId{-1};

    int m_consecutiveDetections{0};
    RecycleCategory m_lastCategory{RecycleCategory::UNKNOWN};
    bool m_itemCounted{false};
};

#endif // RECYCLE_SESSION_CONTROLLER_H
