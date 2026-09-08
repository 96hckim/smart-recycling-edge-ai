/**
 * 키오스크 최상위 윈도우, 화면 스택 라우팅 및 비전/백엔드 이벤트 오케스트레이터 헤더.
 */
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
class RecycleSessionController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 수거함 4개 구역 적재량 게이지 수치 갱신 (종이/캔/페트/비닐)
    void updateBinLevels(int paper, int can, int pet, int vinyl);
    // 값 변경 감지 캐시(`m_cachedBinLevels`)를 적용한 적재 게이지 최적화 갱신
    void updateBinLevels(const BinStatus& status);

public slots:
    // AI 엣지 디바이스 TCP 연결 상태 인디케이터 갱신
    void updateConnectionStatus(bool connected);
    // 상단 상태바 성능 텔레메트리(FPS, 추론시간) 갱신
    void updateTelemetry(double fps, double inferMs);
    // 전체화면 및 창 모드 상호 전환
    void toggleFullScreen();

private slots:
    void onFrameReceived(const QPixmap& pixmap);
    void onMetadataReceived(const FrameMetadata& meta);

    void onMemberStartRequested(const QString& userId);
    void onGuestStartRequested();
    void onRecycleFinished();
    void onReturnToIdle();

    void onUserAuthenticated(int userId, const QString& name, const QString& phone, int currentPoints);
    void onRemoteSessionCancelled();
    void onSubmitCompleted(int logId, int totalPoints);
    void onNetworkError(const QString& errorMessage);

private:
    void initShortcuts();
    void initPages();
    void initJetsonClient();
    void initServerClient();
    void initSessionController();

private:
    Ui::MainWindow* ui;
    IdlePage* m_idlePage { nullptr };
    RecyclePage* m_recyclePage { nullptr };
    ResultPage* m_resultPage { nullptr };
    JetsonClient* m_jetsonClient { nullptr };
    ServerClient* m_serverClient { nullptr };
    RecycleSessionController* m_sessionController { nullptr };

    // 매 프레임 수신되는 불필요한 UI 프로그레스바 리페인트를 방지하기 위한 상태 캐시
    BinStatus m_cachedBinLevels { -1, -1, -1, -1 };
};

#endif // MAINWINDOW_H