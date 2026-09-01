#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QRect>
#include <QString>
#include <QTcpSocket>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 검출 객체 단위 데이터
struct Detection {
    int classId;
    QString className;
    double confidence;
    QRect box;
};

// 프레임 메타데이터 구조체
struct FrameMetadata {
    double timestamp;
    double fps;
    double inferMs;
    QVector<Detection> detections;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnConnect_clicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    Ui::MainWindow* ui;
    QTcpSocket* m_socket;
    QByteArray m_buffer;
    QElapsedTimer m_netTimer;

    void processPacket(const QByteArray& imgBytes, const QByteArray& jsonBytes);
    FrameMetadata parseMetadata(const QByteArray& jsonBytes);
    void renderVideoAndOverlay(const QByteArray& imgBytes, const FrameMetadata& meta, double netLatencyMs);
};

#endif // MAINWINDOW_H