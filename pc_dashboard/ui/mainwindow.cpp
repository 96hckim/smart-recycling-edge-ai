#include "mainwindow.h"
#include "idle_page.h"
#include "recycle_page.h"
#include "result_page.h"
#include "ui_mainwindow.h"
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initPages();

    // 상단 타이틀 더블클릭 이벤트 감지 등록 (키오스크 터치 안전 종료 제스처)
    ui->lblTitle->installEventFilter(this);
    ui->lblTitle->setCursor(Qt::PointingHandCursor);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initPages()
{
    // 1. 3개 메인 페이지 위젯 생성
    m_idlePage = new IdlePage(this);
    m_recyclePage = new RecyclePage(this);
    m_resultPage = new ResultPage(this);

    // 2. QStackedWidget에 순서대로 등록
    ui->stackedWidgetMain->addWidget(m_idlePage); // Index 0
    ui->stackedWidgetMain->addWidget(m_recyclePage); // Index 1
    ui->stackedWidgetMain->addWidget(m_resultPage); // Index 2

    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);

    // 3. IdlePage 시그널 라우팅
    connect(m_idlePage, &IdlePage::sigMemberStartRequested,
        this, &MainWindow::onMemberStartRequested);
    connect(m_idlePage, &IdlePage::sigGuestStartRequested,
        this, &MainWindow::onGuestStartRequested);

    // 4. RecyclePage 시그널 라우팅
    connect(m_recyclePage, &RecyclePage::sigFinishSessionRequested,
        this, &MainWindow::onRecycleFinished);
    connect(m_recyclePage, &RecyclePage::sigCancelSessionRequested,
        this, &MainWindow::onReturnToIdle);

    // 5. ResultPage 시그널 라우팅
    connect(m_resultPage, &ResultPage::sigReturnToIdleRequested,
        this, &MainWindow::onReturnToIdle);
}

void MainWindow::updateBinLevels(int can, int pet, int paper, int general)
{
    ui->progressBarCan->setValue(std::clamp(can, 0, 100));
    ui->progressBarPet->setValue(std::clamp(pet, 0, 100));
    ui->progressBarPaper->setValue(std::clamp(paper, 0, 100));
    ui->progressBarGeneral->setValue(std::clamp(general, 0, 100));
}

void MainWindow::updateConnectionStatus(bool connected)
{
    if (connected) {
        ui->lblConnStatus->setText("● AI VISION ONLINE");
        ui->lblConnStatus->setStyleSheet("background-color: #0F172A; border: 1.5px solid #10B981; "
                                         "border-radius: 12px; padding: 5px 14px; color: #10B981; font-weight: bold; font-size: 12px;");
    } else {
        ui->lblConnStatus->setText("○ AI VISION OFFLINE");
        ui->lblConnStatus->setStyleSheet("background-color: #0F172A; border: 1.5px solid #EF4444; "
                                         "border-radius: 12px; padding: 5px 14px; color: #EF4444; font-weight: bold; font-size: 12px;");
    }
}

void MainWindow::updateTelemetry(double fps, double inferMs, double latencyMs)
{
    ui->lblTelemetry->setText(QString("FPS: %1 | Infer: %2ms | Network Latency: %3ms | Jetson Stream Port: %4")
            .arg(QString::number(fps, 'f', 1))
            .arg(QString::number(inferMs, 'f', 1))
            .arg(QString::number(latencyMs, 'f', 1))
            .arg(Config::JETSON_PORT));
}

void MainWindow::onMemberStartRequested(const QString& userId)
{
    m_currentSession = SessionSummary();
    m_currentSession.isMember = true;
    m_currentSession.userName = userId;

    m_recyclePage->startSession(true, userId);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onGuestStartRequested()
{
    m_currentSession = SessionSummary();
    m_currentSession.isMember = false;

    m_recyclePage->startSession(false);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onRecycleFinished()
{
    // 정산 데이터 빌드 (추후 KioskController 세션 데이터와 직접 연동)
    m_currentSession.canCount = 1;
    m_currentSession.petCount = 2;
    m_currentSession.paperCount = 0;
    m_currentSession.generalCount = 0;
    m_currentSession.totalPoints = (m_currentSession.isMember) ? (1 * Config::POINT_CAN + 2 * Config::POINT_PET) : 0;
    m_currentSession.totalCarbonG = (1 * Config::CARBON_CAN_G + 2 * Config::CARBON_PET_G);

    m_resultPage->showResult(m_currentSession);
    ui->stackedWidgetMain->setCurrentWidget(m_resultPage);
}

void MainWindow::onReturnToIdle()
{
    m_currentSession = SessionSummary();
    m_recyclePage->resetState();
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
}