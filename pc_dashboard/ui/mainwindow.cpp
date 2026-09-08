/**
 * 키오스크 전체 서브시스템 결합, 시그널-슬롯 디스패칭 및 페이지 라우터 구현부.
 */
#include "mainwindow.h"
#include "idle_page.h"
#include "jetson_client.h"
#include "recycle_page.h"
#include "recycle_session_controller.h"
#include "result_page.h"
#include "server_client.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QShortcut>
#include <QStyle>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 순차 서브시스템 의존성 주입 및 통신 채널 기동
    initShortcuts();
    initPages();
    initSessionController();
    initJetsonClient();
    initServerClient();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initShortcuts()
{
    auto* f11Shortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(f11Shortcut, &QShortcut::activated, this, &MainWindow::toggleFullScreen);

    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        }
    });
}

void MainWindow::initPages()
{
    m_idlePage = new IdlePage(this);
    m_recyclePage = new RecyclePage(this);
    m_resultPage = new ResultPage(this);

    ui->stackedWidgetMain->addWidget(m_idlePage);
    ui->stackedWidgetMain->addWidget(m_recyclePage);
    ui->stackedWidgetMain->addWidget(m_resultPage);
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);

    // 대기 화면 -> 배출 세션 화면 진입 라우팅
    connect(m_idlePage, &IdlePage::sigMemberStartRequested, this, &MainWindow::onMemberStartRequested);
    connect(m_idlePage, &IdlePage::sigGuestStartRequested, this, &MainWindow::onGuestStartRequested);

    // 배출 세션 화면 -> 결과 화면 / 세션 취소 라우팅
    connect(m_recyclePage, &RecyclePage::sigFinishSessionRequested, this, &MainWindow::onRecycleFinished);
    connect(m_recyclePage, &RecyclePage::sigCancelSessionRequested, this, &MainWindow::onReturnToIdle);

    // 정산 결과 화면 -> 초기 대기 화면 복귀 라우팅
    connect(m_resultPage, &ResultPage::sigReturnToIdleRequested, this, &MainWindow::onReturnToIdle);
}

void MainWindow::initSessionController()
{
    m_sessionController = new RecycleSessionController(this);

    // 투입 세션 통계 갱신 시 UI 동기화
    connect(m_sessionController, &RecycleSessionController::sigSessionUpdated,
        m_recyclePage, &RecyclePage::updateSessionSummary);

    // 디바운스 확정 및 카메라 BBox 오버레이 좌표 전달
    connect(m_sessionController, &RecycleSessionController::sigDetectionBoxUpdated,
        m_recyclePage, &RecyclePage::updateDetectionState);

    // 투입 안내 배너 상태 동기화
    connect(m_sessionController, &RecycleSessionController::sigGuideBannerRequested,
        this, [this](int type, const QString& text) {
            m_recyclePage->setGuideBanner(static_cast<UITheme::Recycle::BannerType>(type), text);
        });
}

void MainWindow::initJetsonClient()
{
    m_jetsonClient = new JetsonClient(Config::DEFAULT_JETSON_IP, Config::JETSON_PORT, this);

    connect(m_jetsonClient, &JetsonClient::sigConnectionChanged, this, &MainWindow::updateConnectionStatus);
    connect(m_jetsonClient, &JetsonClient::sigFrameReceived, this, &MainWindow::onFrameReceived);
    connect(m_jetsonClient, &JetsonClient::sigMetadataReceived, this, &MainWindow::onMetadataReceived);
    connect(m_jetsonClient, &JetsonClient::sigTelemetryUpdated, this, &MainWindow::updateTelemetry);

    m_jetsonClient->connectToJetson();
}

void MainWindow::initServerClient()
{
    m_serverClient = new ServerClient(Config::DEFAULT_BIN_ID, Config::DEFAULT_BACKEND_HOST, Config::DEFAULT_BACKEND_PORT, this);

    connect(m_serverClient, &ServerClient::userAuthenticated, this, &MainWindow::onUserAuthenticated);
    connect(m_serverClient, &ServerClient::sessionCancelled, this, &MainWindow::onRemoteSessionCancelled);
    connect(m_serverClient, &ServerClient::submitCompleted, this, &MainWindow::onSubmitCompleted);
    connect(m_serverClient, &ServerClient::networkErrorOccurred, this, &MainWindow::onNetworkError);

    m_serverClient->connectToKioskSocket();
}

void MainWindow::onUserAuthenticated(int userId, const QString& name, const QString& phone, int currentPoints)
{
    Q_UNUSED(phone);
    Q_UNUSED(currentPoints);
    qDebug() << "[MainWindow] 모바일 QR 인증 감지: ID =" << userId << ", Name =" << name;

    // 대기 화면 상태일 때 모바일 앱 QR 스캔이 인입되면 즉시 사용자 맞춤 세션 개시
    if (ui->stackedWidgetMain->currentWidget() == m_idlePage) {
        m_sessionController->startSession(true, name, userId);
        m_recyclePage->startSession(true, name);
        ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
    }
}

void MainWindow::onRemoteSessionCancelled()
{
    qDebug() << "[MainWindow] 모바일 앱 또는 원격에 의한 세션 취소 수신. 대기 화면으로 복귀합니다.";
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage) {
        m_sessionController->cancelSession();
        m_recyclePage->resetState();
        ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
    }
}

void MainWindow::onSubmitCompleted(int logId, int totalPoints)
{
    qDebug() << "[MainWindow] 서버 정산 완료 수신: Log ID =" << logId << ", Total Points =" << totalPoints;
}

void MainWindow::onNetworkError(const QString& errorMessage)
{
    qWarning() << "[MainWindow] 백엔드 서버 네트워크 오류:" << errorMessage;
}

void MainWindow::onFrameReceived(const QPixmap& pixmap)
{
    // 불필요한 GPU/CPU 렌더링 낭비를 막기 위해 활성 배출 화면일 때만 프레임 갱신
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage) {
        m_recyclePage->updateFrame(pixmap);
    }
}

void MainWindow::onMetadataReceived(const FrameMetadata& meta)
{
    // 1. 하단 물리 수거함 수위 게이지 UI 갱신 (캐시 비교 필터링)
    updateBinLevels(meta.binLevels);

    // 2. 실시간 투입 세션 중일 때만 FSM에 비전 및 도어 텔레메트리 디스패칭
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage && m_sessionController) {
        m_sessionController->processFrameMetadata(meta);
    }
}

void MainWindow::updateBinLevels(const BinStatus& status)
{
    // 수거함 적재 레벨 변동이 없을 때 위젯 리페인트(Repaint) 오버헤드 차단
    if (m_cachedBinLevels == status) {
        return;
    }
    m_cachedBinLevels = status;

    ui->progressBarPaper->setValue(std::clamp(status.paper, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarCan->setValue(std::clamp(status.can, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarPet->setValue(std::clamp(status.pet, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarVinyl->setValue(std::clamp(status.vinyl, 0, Config::MAX_BIN_CAPACITY));
}

void MainWindow::updateBinLevels(int paper, int can, int pet, int vinyl)
{
    BinStatus status;
    status.paper = paper;
    status.can = can;
    status.pet = pet;
    status.vinyl = vinyl;
    updateBinLevels(status);
}

void MainWindow::updateConnectionStatus(bool connected)
{
    ui->lblConnStatus->setText(connected ? UITheme::Header::STATUS_ONLINE : UITheme::Header::STATUS_OFFLINE);
    ui->lblConnStatus->setProperty("online", connected);
    ui->lblConnStatus->style()->unpolish(ui->lblConnStatus);
    ui->lblConnStatus->style()->polish(ui->lblConnStatus);
}

void MainWindow::updateTelemetry(double fps, double inferMs)
{
    ui->lblTelemetry->setText(QString(UITheme::Header::TELEMETRY_FMT)
            .arg(QString::number(fps, 'f', 1))
            .arg(QString::number(inferMs, 'f', 1)));
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::onMemberStartRequested(const QString& userId)
{
    m_sessionController->startSession(true, userId);
    m_recyclePage->startSession(true, userId);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onGuestStartRequested()
{
    m_sessionController->startSession(false);
    m_recyclePage->startSession(false);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onRecycleFinished()
{
    const SessionSummary summary = m_sessionController->sessionSummary();
    const int userId = m_sessionController->currentUserId();

    // 결과 정산 페이지 전환 및 영수증 렌더링
    m_resultPage->showResult(summary);
    ui->stackedWidgetMain->setCurrentWidget(m_resultPage);

    // 중앙 관제 서버로 최종 배출량 및 리워드 REST API 비동기 전송
    RecycleCounts counts;
    counts.paper = summary.paperCount;
    counts.can = summary.canCount;
    counts.pet = summary.petCount;
    counts.vinyl = summary.vinylCount;

    if (m_serverClient) {
        m_serverClient->submitRecycleResult(userId, counts, summary.totalCarbonG, summary.totalPoints);
    }

    m_sessionController->finishSession();
}

void MainWindow::onReturnToIdle()
{
    // 활성 투입 화면에서 키오스크 UI를 통해 취소된 경우 백엔드에 세션 취소 통지 (모바일 동기화)
    if (m_serverClient && ui->stackedWidgetMain->currentWidget() == m_recyclePage) {
        int currentUserId = m_sessionController ? m_sessionController->currentUserId() : -1;
        m_serverClient->cancelRecycleSession(currentUserId);
    }

    m_sessionController->cancelSession();
    m_recyclePage->resetState();
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
}