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

    // 공통 수거함 상태 갱신 슬롯/함수
    void updateBinLevels(int can, int pet, int paper, int general);

private slots:
    void onMemberStartRequested(const QString& userId);
    void onGuestStartRequested();
    void onRecycleFinished();
    void onReturnToIdle();
    void updateConnectionStatus(bool connected);

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