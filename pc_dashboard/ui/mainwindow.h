#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "app_config.h"
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class IdlePage;
class RecyclePage;
class ResultPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 공통 하단 수거함 적재 레벨 갱신 (CAN, PET, PAPER, GENERAL: 0~100%)
    void updateBinLevels(int can, int pet, int paper, int general);

    // 상단 네트워크 연결 상태 뱃지 갱신
    void updateConnectionStatus(bool connected);

    // 최하단 실시간 텔레메트리 정보 갱신
    void updateTelemetry(double fps, double inferMs, double latencyMs);

private slots:
    void onMemberStartRequested(const QString& userId);
    void onGuestStartRequested();
    void onRecycleFinished();
    void onReturnToIdle();

private:
    void initPages();

private:
    Ui::MainWindow* ui;
    IdlePage* m_idlePage { nullptr };
    RecyclePage* m_recyclePage { nullptr };
    ResultPage* m_resultPage { nullptr };

    SessionSummary m_currentSession;
};

#endif // MAINWINDOW_H