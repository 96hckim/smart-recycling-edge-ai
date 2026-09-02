#include "mainwindow.h"
#include "idle_page.h"
#include "jetson_client.h"
#include "recycle_page.h"
#include "result_page.h"
#include "ui_mainwindow.h"
#include <QStyle>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initPages();
    initJetsonClient();
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

void MainWindow::initJetsonClient()
{
    m_jetsonClient = new JetsonClient(Config::DEFAULT_JETSON_IP, Config::JETSON_PORT, this);

    connect(m_jetsonClient, &JetsonClient::sigConnectionChanged, this, &MainWindow::updateConnectionStatus);
    connect(m_jetsonClient, &JetsonClient::sigFrameReceived, this, &MainWindow::onFrameReceived);
    connect(m_jetsonClient, &JetsonClient::sigMetadataReceived, this, &MainWindow::onMetadataReceived);
    connect(m_jetsonClient, &JetsonClient::sigTelemetryUpdated, this, &MainWindow::updateTelemetry);

    m_jetsonClient->connectToJetson();
}

void MainWindow::onFrameReceived(const QPixmap& pixmap)
{
    if (ui->stackedWidgetMain->currentWidget() == m_recyclePage) {
        m_recyclePage->updateFrame(pixmap);
    }
}

void MainWindow::onMetadataReceived(const FrameMetadata& meta)
{
    if (ui->stackedWidgetMain->currentWidget() != m_recyclePage)
        return;

    if (meta.detections.isEmpty()) {
        resetDetectionState();
        m_recyclePage->updateDetectionState("", 0.0, 0);
        return;
    }

    const Detection& top = meta.detections.first();

    if (top.category != RecycleCategory::UNKNOWN && top.category == m_lastDetectedCategory) {
        m_consecutiveDetections++;
    } else {
        m_lastDetectedCategory = top.category;
        m_consecutiveDetections = 1;
        m_doorOpenedForCurrentItem = false;
    }

    m_recyclePage->updateDetectionState(top.className, top.confidence, m_consecutiveDetections, top.box);

    if (m_consecutiveDetections >= Config::STABLE_FRAME_THRESHOLD && !m_doorOpenedForCurrentItem) {
        m_doorOpenedForCurrentItem = true;
        m_currentSession.addItem(top.category, 1);
        m_recyclePage->updateSessionSummary(m_currentSession);
        openBinDoor(top.category);
    }
}

void MainWindow::openBinDoor(RecycleCategory category)
{
    if (m_jetsonClient && m_jetsonClient->isConnected()) {
        m_jetsonClient->sendOpenBinCommand(category);
    }
}

void MainWindow::resetDetectionState()
{
    m_consecutiveDetections = 0;
    m_lastDetectedCategory = RecycleCategory::UNKNOWN;
    m_doorOpenedForCurrentItem = false;
}

void MainWindow::updateBinLevels(int can, int pet, int paper, int general)
{
    ui->progressBarCan->setValue(std::clamp(can, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarPet->setValue(std::clamp(pet, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarPaper->setValue(std::clamp(paper, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarGeneral->setValue(std::clamp(general, 0, Config::MAX_BIN_CAPACITY));
}

void MainWindow::updateConnectionStatus(bool connected)
{
    // ★ 상수로 대체
    ui->lblConnStatus->setText(connected ? Config::Connection::STATUS_ONLINE : Config::Connection::STATUS_OFFLINE);
    ui->lblConnStatus->setProperty("online", connected);
    ui->lblConnStatus->style()->unpolish(ui->lblConnStatus);
    ui->lblConnStatus->style()->polish(ui->lblConnStatus);
}

void MainWindow::updateTelemetry(double fps, double inferMs, double latencyMs)
{
    // ★ 상수로 대체
    ui->lblTelemetry->setText(QString(Config::Telemetry::FORMAT_STR)
            .arg(QString::number(fps, 'f', 1))
            .arg(QString::number(inferMs, 'f', 1))
            .arg(QString::number(latencyMs, 'f', 1))
            .arg(Config::JETSON_PORT));
}

void MainWindow::onMemberStartRequested(const QString& userId)
{
    m_currentSession.reset();
    m_currentSession.isMember = true;
    m_currentSession.userName = userId;
    resetDetectionState();

    m_recyclePage->startSession(true, userId);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onGuestStartRequested()
{
    m_currentSession.reset();
    m_currentSession.isMember = false;
    resetDetectionState();

    m_recyclePage->startSession(false);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onRecycleFinished()
{
    m_resultPage->showResult(m_currentSession);
    ui->stackedWidgetMain->setCurrentWidget(m_resultPage);
}

void MainWindow::onReturnToIdle()
{
    m_currentSession.reset();
    resetDetectionState();

    m_recyclePage->resetState();
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
}