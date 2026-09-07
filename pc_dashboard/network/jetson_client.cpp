#include "jetson_client.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

JetsonClient::JetsonClient(const QString& host, quint16 port, QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_host(host)
    , m_port(port)
{
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, Config::SOCKET_BUFFER_RESERVE);

    connect(m_socket, &QTcpSocket::connected, this, &JetsonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &JetsonClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &JetsonClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &JetsonClient::onSocketError);

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
    while (m_rxBuffer.size() >= static_cast<int>(Config::HEADER_SIZE)) {
        const uchar* buf = reinterpret_cast<const uchar*>(m_rxBuffer.constData());

        // 8B Big-Endian Header: [img_len(4B)][json_len(4B)]
        const quint32 imgSize = qFromBigEndian<quint32>(buf);
        const quint32 jsonSize = qFromBigEndian<quint32>(buf + 4);

        // 비정상 크기 감지 시 버퍼 리셋
        if (imgSize > Config::MAX_IMAGE_SIZE || jsonSize > Config::MAX_JSON_SIZE) {
            qWarning() << "[TCP] 비정상 패킷 헤더 -> 버퍼 초기화";
            m_rxBuffer.clear();
            return;
        }

        const int totalPacketSize = static_cast<int>(Config::HEADER_SIZE + imgSize + jsonSize);
        if (m_rxBuffer.size() < totalPacketSize) {
            return; // 미완성 패킷 대기
        }

        // 1. 영상 언패킹
        if (imgSize > 0) {
            QPixmap pixmap;
            if (pixmap.loadFromData(buf + Config::HEADER_SIZE, imgSize, "JPG")) {
                emit sigFrameReceived(pixmap);
            }
        }

        // 2. 메타데이터 언패킹
        if (jsonSize > 0) {
            const char* jsonPtr = reinterpret_cast<const char*>(buf + Config::HEADER_SIZE + imgSize);
            processJsonMeta(QByteArray::fromRawData(jsonPtr, static_cast<int>(jsonSize)));
        }

        // 3. 소비된 패킷 바이트 제거
        m_rxBuffer.remove(0, totalPacketSize);
    }
}

void JetsonClient::processJsonMeta(const QByteArray& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        return;
    }

    FrameMetadata meta = FrameMetadata::fromJson(doc.object());

    const double nowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const double latencyMs = qMax(0.0, (nowSec - meta.timestamp) * 1000.0);

    emit sigMetadataReceived(meta);
    emit sigTelemetryUpdated(meta.fps, meta.inferMs, latencyMs);
}