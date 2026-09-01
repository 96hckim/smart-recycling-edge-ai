#include "mainwindow.h"
#include "idle_page.h"
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
    ui->progressBarCan->setValue(std::clamp(can, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarPet->setValue(std::clamp(pet, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarPaper->setValue(std::clamp(paper, 0, Config::MAX_BIN_CAPACITY));
    ui->progressBarGeneral->setValue(std::clamp(general, 0, Config::MAX_BIN_CAPACITY));
}

void MainWindow::updateConnectionStatus(bool connected)
{
    // 텍스트 설정
    ui->lblConnStatus->setText(connected ? "● AI VISION ONLINE" : "○ AI VISION OFFLINE");

    // 동적 프로퍼티(online) 트리거로 QSS 색상 자동 반영 (인라인 폰트 크기 덮어쓰기 방지)
    ui->lblConnStatus->setProperty("online", connected);
    ui->lblConnStatus->style()->unpolish(ui->lblConnStatus);
    ui->lblConnStatus->style()->polish(ui->lblConnStatus);
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
    m_currentSession.reset();
    m_currentSession.isMember = true;
    m_currentSession.userName = userId;

    m_recyclePage->startSession(true, userId);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onGuestStartRequested()
{
    m_currentSession.reset();
    m_currentSession.isMember = false;

    m_recyclePage->startSession(false);
    ui->stackedWidgetMain->setCurrentWidget(m_recyclePage);
}

void MainWindow::onRecycleFinished()
{
    // SessionSummary 내부의 addItem() 호출 시 포인트 및 탄소 절감량 자동 계산
    m_currentSession.addItem(RecycleCategory::CAN, 1);
    m_currentSession.addItem(RecycleCategory::PET, 2);

    m_resultPage->showResult(m_currentSession);
    ui->stackedWidgetMain->setCurrentWidget(m_resultPage);
}

void MainWindow::onReturnToIdle()
{
    m_currentSession.reset();
    m_recyclePage->resetState();
    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);
}