#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_config.h"
#include <QMainWindow>
#include <QPixmap>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class IdlePage;
class RecyclePage;
class ResultPage;
class JetsonClient;
class ServerClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 순서: 종이 -> 캔 -> 페트 -> 비닐
    void updateBinLevels(int paper, int can, int pet, int vinyl);

public slots:
    void updateConnectionStatus(bool connected);
    void updateTelemetry(double fps, double inferMs, double latencyMs);

private slots:
    // Jetson Client Slots
    void onFrameReceived(const QPixmap& pixmap);
    void onMetadataReceived(const FrameMetadata& meta);

    // Page Navigation Slots
    void onMemberStartRequested(const QString& userId);
    void onGuestStartRequested();
    void onRecycleFinished();
    void onReturnToIdle();

    // FastAPI Server Client Slots
    void onUserAuthenticated(int userId, const QString& name, const QString& phone, int currentPoints);
    void onSubmitCompleted(int logId, int totalPoints);
    void onNetworkError(const QString& errorMessage);

private:
    void initPages();
    void initJetsonClient();
    void initServerClient();
    void resetDetectionState();
    void openBinDoor(RecycleCategory category);

private:
    Ui::MainWindow* ui;
    IdlePage* m_idlePage { nullptr };
    RecyclePage* m_recyclePage { nullptr };
    ResultPage* m_resultPage { nullptr };
    JetsonClient* m_jetsonClient { nullptr };
    ServerClient* m_serverClient { nullptr };

    SessionSummary m_currentSession;

    int m_consecutiveDetections { 0 };
    RecycleCategory m_lastDetectedCategory { RecycleCategory::UNKNOWN };
    bool m_doorOpenedForCurrentItem { false };

    int m_currentUserId { -1 };
};

#endif // MAINWINDOW_H