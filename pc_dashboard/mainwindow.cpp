#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QtEndian>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    // 고속 패킷 수신을 위한 버퍼 메모리 사전 확보 (512KB)
    m_buffer.reserve(512 * 1024);

    connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &MainWindow::onSocketError);

    m_netTimer.start();
}

MainWindow::~MainWindow()
{
    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
    delete ui;
}

void MainWindow::on_btnConnect_clicked()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    } else {
        const QString ip = ui->edtIp->text().trimmed();
        if (ip.isEmpty()) {
            ui->lblInfo->setText("IP를 입력하세요.");
            return;
        }

        m_buffer.clear();
        ui->btnConnect->setEnabled(false);
        ui->lblInfo->setText("연결 시도 중...");

        m_socket->connectToHost(ip, 9000);
        m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // TCP_NODELAY (지연 제거)
    }
}

void MainWindow::onSocketConnected()
{
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setText("Disconnect");
    ui->lblInfo->setText("Connected (Streaming)");
    qDebug() << "[NET] Jetson 서버 연결 성공";
    m_netTimer.restart();
}

void MainWindow::onSocketDisconnected()
{
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setText("Connect");
    ui->lblInfo->setText("Disconnected");
    ui->lblVideo->setText("영상 대기 중");
    qDebug() << "[NET] Jetson 서버 연결 종료";
    m_buffer.clear();
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setText("Connect");
    ui->lblInfo->setText(QString("연결 오류: %1").arg(m_socket->errorString()));
}

void MainWindow::onSocketReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 8바이트 헤더 파싱 (이미지 바이트수 4B + JSON 바이트수 4B)
    while (m_buffer.size() >= 8) {
        const uchar* data = reinterpret_cast<const uchar*>(m_buffer.constData());
        const quint32 imgSize = qFromBigEndian<quint32>(data);
        const quint32 jsonSize = qFromBigEndian<quint32>(data + 4);

        const quint32 totalPacketSize = 8 + imgSize + jsonSize;
        if (static_cast<quint32>(m_buffer.size()) < totalPacketSize) {
            break; // 전체 패킷이 도착할 때까지 대기
        }

        // 제로카피 뷰 대신 추출 후 버퍼 즉시 슬라이싱
        const QByteArray imgBytes = m_buffer.mid(8, imgSize);
        const QByteArray jsonBytes = m_buffer.mid(8 + imgSize, jsonSize);
        m_buffer.remove(0, totalPacketSize);

        processPacket(imgBytes, jsonBytes);
    }
}

void MainWindow::processPacket(const QByteArray& imgBytes, const QByteArray& jsonBytes)
{
    // 순수 패킷 도달 주기(지연시간) 측정
    const double netLatencyMs = static_cast<double>(m_netTimer.restart());

    // 1. JSON 메타데이터 파싱
    const FrameMetadata meta = parseMetadata(jsonBytes);

    // 2. 화면 렌더링 및 박스 오버레이 출력
    renderVideoAndOverlay(imgBytes, meta, netLatencyMs);
}

FrameMetadata MainWindow::parseMetadata(const QByteArray& jsonBytes)
{
    FrameMetadata meta;
    meta.timestamp = 0.0;
    meta.fps = 0.0;
    meta.inferMs = 0.0;

    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) {
        return meta;
    }

    const QJsonObject root = doc.object();
    meta.fps = root["fps"].toDouble();
    meta.inferMs = root["infer_ms"].toDouble();
    meta.timestamp = root["timestamp"].toDouble();

    const QJsonArray detArray = root["detections"].toArray();
    meta.detections.reserve(detArray.size());

    for (const QJsonValue& val : detArray) {
        const QJsonObject detObj = val.toObject();
        const QJsonArray boxArray = detObj["box"].toArray();

        if (boxArray.size() == 4) {
            const int x1 = boxArray[0].toInt();
            const int y1 = boxArray[1].toInt();
            const int x2 = boxArray[2].toInt();
            const int y2 = boxArray[3].toInt();

            Detection det;
            det.classId = detObj["class_id"].toInt();
            det.className = detObj["class_name"].toString();
            det.confidence = detObj["confidence"].toDouble();
            det.box = QRect(QPoint(x1, y1), QPoint(x2, y2));
            meta.detections.append(det);
        }
    }

    return meta;
}

void MainWindow::renderVideoAndOverlay(const QByteArray& imgBytes, const FrameMetadata& meta, double netLatencyMs)
{
    // 1. JPEG -> QPixmap 직접 디코딩 (QImage 변환 생략)
    QPixmap pixmap;
    if (!pixmap.loadFromData(reinterpret_cast<const uchar*>(imgBytes.constData()), imgBytes.size(), "JPG")) {
        return;
    }

    // 2. QPainter로 원본 Pixmap 위에 바운딩 박스 및 라벨 오버레이
    if (!meta.detections.isEmpty()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setFont(QFont("Pretendard", 11, QFont::Bold));

        for (const Detection& det : meta.detections) {
            // 박스 렌더링
            painter.setPen(QPen(QColor(0, 230, 118), 2)); // 형광 그린
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(det.box);

            // 라벨 텍스트 배경 및 문자열
            const QString labelText = QString("%1 %2%").arg(det.className).arg(static_cast<int>(det.confidence * 100));
            const QFontMetrics fm(painter.font());
            const QRect textRect = fm.boundingRect(labelText);

            const QPoint textPos(det.box.left(), qMax(textRect.height() + 4, det.box.top() - 4));
            const QRect bgRect(textPos.x(), textPos.y() - textRect.height(), textRect.width() + 8, textRect.height() + 4);

            painter.fillRect(bgRect, QColor(0, 0, 0, 160));
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(textPos.x() + 4, textPos.y() - 2, labelText);
        }
        painter.end();
    }

    // 3. UI 갱신 (FastTransformation으로 고속 렌더링)
    ui->lblVideo->setPixmap(pixmap.scaled(
        ui->lblVideo->size(),
        Qt::KeepAspectRatio,
        Qt::FastTransformation));

    // 4. 관제 정보 라벨 갱신
    ui->lblInfo->setText(QString("FPS: %1 | Infer: %2ms | Latency: %3ms")
            .arg(meta.fps, 4, 'f', 1)
            .arg(meta.inferMs, 4, 'f', 1)
            .arg(netLatencyMs, 4, 'f', 1));
}