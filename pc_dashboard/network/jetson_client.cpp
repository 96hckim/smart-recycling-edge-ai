#include "jetson_client.h"
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

JetsonClient::JetsonClient(const QString& host, quint16 port, QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_host(host)
    , m_port(port)
{
    // 수신 버퍼 512KB 확장
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, Config::SOCKET_BUFFER_RESERVE);

    connect(m_socket, &QTcpSocket::connected, this, &JetsonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &JetsonClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &JetsonClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &JetsonClient::onSocketError);

    // 자동 재연결 타이머 설정
    m_reconnectTimer->setInterval(Config::AUTO_RECONNECT_INTERVAL_MS);
    connect(m_reconnectTimer, &QTimer::timeout, this, &JetsonClient::onReconnectTimeout);
}

JetsonClient::~JetsonClient()
{
    disconnectFromJetson();
}

void JetsonClient::connectToJetson()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "[TCP] Jetson 연결 시도 ->" << m_host << ":" << m_port;
        m_socket->connectToHost(m_host, m_port);
    }
}

void JetsonClient::disconnectFromJetson()
{
    m_reconnectTimer->stop();
    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
}

bool JetsonClient::isConnected() const
{
    return (m_socket && m_socket->state() == QAbstractSocket::ConnectedState);
}

void JetsonClient::onSocketConnected()
{
    qDebug() << "[TCP] Jetson 서버 연결 성공!";
    m_reconnectTimer->stop();
    m_rxBuffer.clear();
    emit sigConnectionChanged(true);
}

void JetsonClient::onSocketDisconnected()
{
    qWarning() << "[TCP] Jetson 연결 단절. 재연결 대기 중...";
    emit sigConnectionChanged(false);
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void JetsonClient::onReconnectTimeout()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(m_host, m_port);
    }
}

void JetsonClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qWarning() << "[TCP Socket Error]" << m_socket->errorString();
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void JetsonClient::onReadyRead()
{
    m_rxBuffer.append(m_socket->readAll());
    parseBuffer();
}

void JetsonClient::parseBuffer()
{
    while (true) {
        // 1. 헤더(8B) 수신 확인
        if (m_rxBuffer.size() < static_cast<int>(Config::HEADER_SIZE)) {
            return;
        }

        // 2. Big-Endian uint32 2개(imgSize, jsonSize) 파싱
        quint32 imgSize = 0;
        quint32 jsonSize = 0;
        QDataStream stream(m_rxBuffer.left(Config::HEADER_SIZE));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> imgSize >> jsonSize;

        const int totalPacketSize = static_cast<int>(Config::HEADER_SIZE + imgSize + jsonSize);

        // 3. 페이로드 전체 도착 대기
        if (m_rxBuffer.size() < totalPacketSize) {
            return;
        }

        // 4. 데이터 분리 추출
        const QByteArray imgBytes = m_rxBuffer.mid(Config::HEADER_SIZE, imgSize);
        const QByteArray jsonBytes = m_rxBuffer.mid(Config::HEADER_SIZE + imgSize, jsonSize);

        // 버퍼 소비
        m_rxBuffer.remove(0, totalPacketSize);

        // 5. 영상 디코딩 (JPEG)
        if (!imgBytes.isEmpty()) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reinterpret_cast<const uchar*>(imgBytes.constData()), imgBytes.size(), "JPG")) {
                emit sigFrameReceived(pixmap);
            }
        }

        // 6. JSON 메타데이터 파싱
        if (!jsonBytes.isEmpty()) {
            processJsonMeta(jsonBytes);
        }
    }
}

void JetsonClient::processJsonMeta(const QByteArray& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    FrameMetadata meta;
    meta.timestamp = root.value("timestamp").toDouble();
    meta.fps = root.value("fps").toDouble();
    meta.inferMs = root.value("infer_ms").toDouble();

    // 네트워크 레이턴시 계산 (초 단위 epoch 비교)
    const double nowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const double latencyMs = qMax(0.0, (nowSec - meta.timestamp) * 1000.0);

    QJsonArray detArray = root.value("detections").toArray();
    for (const QJsonValue& val : detArray) {
        QJsonObject dObj = val.toObject();
        Detection d;
        d.classId = dObj.value("class_id").toInt();
        d.className = dObj.value("class_name").toString();
        d.confidence = dObj.value("confidence").toDouble();
        d.category = Config::parseCategory(d.className);

        // Box 형식 파싱
        const QJsonArray bArr = dObj.value("box").toArray();
        if (bArr.size() >= 4) {
            d.box = QRect(QPoint(bArr[0].toInt(), bArr[1].toInt()),
                QPoint(bArr[2].toInt(), bArr[3].toInt()));
        }

        meta.detections.append(d);
    }

    emit sigMetadataReceived(meta);
    emit sigTelemetryUpdated(meta.fps, meta.inferMs, latencyMs);
}

void JetsonClient::sendOpenBinCommand(RecycleCategory category)
{
    sendOpenBinCommand(Config::getCategoryNameEn(category));
}

void JetsonClient::sendOpenBinCommand(const QString& targetCategory)
{
    if (!isConnected()) {
        qWarning() << "[TCP TX 실패] Jetson 미연결 상태";
        return;
    }

    QJsonObject cmdObj;
    cmdObj[Config::KEY_CMD] = Config::CMD_OPEN_BIN;
    cmdObj[Config::KEY_TARGET] = targetCategory.toUpper();

    const QByteArray packet = QJsonDocument(cmdObj).toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(packet);
    m_socket->flush();
    qDebug() << "[TCP TX -> Jetson]" << packet.trimmed();
}