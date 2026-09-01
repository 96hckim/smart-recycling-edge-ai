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
    // OS 기본 타이틀바와 외곽 테두리 완전 제거
    // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    ui->setupUi(this);
    initPages();
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

    ui->stackedWidgetMain->addWidget(m_idlePage); // Index 0
    ui->stackedWidgetMain->addWidget(m_recyclePage); // Index 1
    ui->stackedWidgetMain->addWidget(m_resultPage); // Index 2

    ui->stackedWidgetMain->setCurrentWidget(m_idlePage);

    // IdlePage 시그널
    connect(m_idlePage, &IdlePage::sigMemberStartRequested, this, &MainWindow::onMemberStartRequested);
    connect(m_idlePage, &IdlePage::sigGuestStartRequested, this, &MainWindow::onGuestStartRequested);

    // RecyclePage 시그널
    connect(m_recyclePage, &RecyclePage::sigFinishSessionRequested, this, &MainWindow::onRecycleFinished);
    connect(m_recyclePage, &RecyclePage::sigCancelSessionRequested, this, &MainWindow::onReturnToIdle);

    // ResultPage 시그널
    connect(m_resultPage, &ResultPage::sigReturnToIdleRequested, this, &MainWindow::onReturnToIdle);
}

void MainWindow::updateBinLevels(int can, int pet, int paper, int general)
{
    ui->progressBarCan->setValue(std::clamp(can, 0, 100));
    ui->progressBarPet->setValue(std::clamp(pet, 0, 100));
    ui->progressBarPaper->setValue(std::clamp(paper, 0, 100));
    ui->progressBarGeneral->setValue(std::clamp(general, 0, 100));
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
    // 테스트용 샘플 정산 데이터
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