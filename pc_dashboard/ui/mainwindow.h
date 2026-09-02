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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void updateBinLevels(int can, int pet, int paper, int general);

public slots:
    void updateConnectionStatus(bool connected);
    void updateTelemetry(double fps, double inferMs, double latencyMs);

private slots:
    void onFrameReceived(const QPixmap& pixmap);
    void onMetadataReceived(const FrameMetadata& meta);

    void onMemberStartRequested(const QString& userId);
    void onGuestStartRequested();
    void onRecycleFinished();
    void onReturnToIdle();

private:
    void initPages();
    void initJetsonClient();
    void resetDetectionState();
    void openBinDoor(RecycleCategory category);

private:
    Ui::MainWindow* ui;
    IdlePage* m_idlePage { nullptr };
    RecyclePage* m_recyclePage { nullptr };
    ResultPage* m_resultPage { nullptr };
    JetsonClient* m_jetsonClient { nullptr };

    SessionSummary m_currentSession;

    int m_consecutiveDetections { 0 };
    RecycleCategory m_lastDetectedCategory { RecycleCategory::UNKNOWN };
    bool m_doorOpenedForCurrentItem { false };
};

#endif // MAINWINDOW_H