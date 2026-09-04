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
    while (true) {
        if (m_rxBuffer.size() < static_cast<int>(Config::HEADER_SIZE)) {
            return;
        }

        quint32 imgSize = 0;
        quint32 jsonSize = 0;
        QDataStream stream(m_rxBuffer.left(Config::HEADER_SIZE));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> imgSize >> jsonSize;

        const int totalPacketSize = static_cast<int>(Config::HEADER_SIZE + imgSize + jsonSize);

        if (m_rxBuffer.size() < totalPacketSize) {
            return;
        }

        const QByteArray imgBytes = m_rxBuffer.mid(Config::HEADER_SIZE, imgSize);
        const QByteArray jsonBytes = m_rxBuffer.mid(Config::HEADER_SIZE + imgSize, jsonSize);

        m_rxBuffer.remove(0, totalPacketSize);

        if (!imgBytes.isEmpty()) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reinterpret_cast<const uchar*>(imgBytes.constData()), imgBytes.size(), "JPG")) {
                emit sigFrameReceived(pixmap);
            }
        }

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

    FrameMetadata meta = FrameMetadata::fromJson(doc.object());

    const double nowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const double latencyMs = qMax(0.0, (nowSec - meta.timestamp) * 1000.0);

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