#include "mainwindow.h"
#include "idle_page.h"
#include "jetson_client.h"
#include "recycle_page.h"
#include "recycle_session_controller.h"
#include "result_page.h"
#include "server_client.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QStyle>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initPages();
    initSessionController();
    initJetsonClient();
    initServerClient();
}

MainWindow::~MainWindow()
{
    delete ui;
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

    connect(m_idlePage, &IdlePage::sigMemberStartRequested, this, &MainWindow::onMemberStartRequested);
    connect(m_idlePage, &IdlePage::sigGuestStartRequested, this, &MainWindow::onGuestStartRequested);

    connect(m_recyclePage, &RecyclePage::sigFinishSessionRequested, this, &MainWindow::onRecycleFinished);
    connect(m_recyclePage, &RecyclePage::sigCancelSessionRequested, this, &MainWindow::onReturnToIdle);

    connect(m_resultPage, &ResultPage::sigReturnToIdleRequested, this, &MainWindow::onReturnToIdle);
}

void MainWindow::initSessionController()
{
    m_sessionController = new RecycleSessionController(this);

    // 세션 카운트 및 통계 갱신
    connect(m_sessionController, &RecycleSessionController::sigSessionUpdated,
            m_recyclePage, &RecyclePage::updateSessionSummary);

    // 카메라 바운딩 박스 오버레이
    connect(m_sessionController, &RecycleSessionController::sigDetectionBoxUpdated,
            m_recyclePage, &RecyclePage::updateDetectionState);

    // 가이드 배너 동기화
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
    connect(m_serverClient, &ServerClient::submitCompleted, this, &MainWindow::onSubmitCompleted);
    connect(m_serverClient, &ServerClient::networkErrorOccurred, this, &MainWindow::onNetworkError);

    m_serverClient->connectToKioskSocket();
}

void MainWindow::onUserAuthenticated(int userId, const QString& name, const QString& phone, int currentPoints)
{
    Q_UNUSED(phone);
    Q_UNUSED(currentPoints);
    qDebug() << "[MainWindow] 모바일 QR 인증 감지: ID =" << userId << ", Name =" << name;

    if (ui->stackedWidgetMain->currentWidget() == m_idlePage) {
        m_sessionController->startSession(true, name, userId);
        m_recyclePage->startSession(true, name);
        ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
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
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage) {
        m_recyclePage->updateFrame(pixmap);
    }
}

void MainWindow::onMetadataReceived(const FrameMetadata& meta)
{
    // 1. 하단 적재함 수위 게이지 캐시 기반 갱신
    updateBinLevels(meta.binLevels);

    // 2. 투입 세션 화면(RecyclePage)일 때만 세션 컨트롤러에 전달
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage && m_sessionController) {
        m_sessionController->processFrameMetadata(meta);
    }
}

// 순서: 종이 -> 캔 -> 페트 -> 비닐
void MainWindow::updateBinLevels(const BinStatus& status)
{
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

void MainWindow::updateTelemetry(double fps, double inferMs, double latencyMs)
{
    ui->lblTelemetry->setText(QString(UITheme::Header::TELEMETRY_FMT)
            .arg(QString::number(fps, 'f', 1))
            .arg(QString::number(inferMs, 'f', 1))
            .arg(QString::number(latencyMs, 'f', 1))
            .arg(Config::JETSON_PORT));
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

    m_resultPage->showResult(summary);
    ui->stackedWidgetMain->setCurrentWidget(m_resultPage);

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
    m_sessionController->cancelSession();
    m_recyclePage->resetState();
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
}