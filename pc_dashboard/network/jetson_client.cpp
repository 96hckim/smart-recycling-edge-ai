/**
 * Jetson TCP 이진 패킷 스트림 파싱 및 0-Copy 기반 텔레메트리 처리 구현부.
 */
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
    // 고해상도 프레임 유입 시 OS 소켓 버퍼 오버플로우 방지용 수신 버퍼 확장
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
    // 네트워크 오류로 패킷 동기화가 깨졌을 때 메모리 무한 누적으로 인한 OOM 방지
    if (m_rxBuffer.size() > Config::MAX_BUFFER_CAPACITY) {
        qWarning() << "[TCP] 수신 버퍼 최대 허용량 초과 -> 버퍼 초기화";
        m_rxBuffer.clear();
        return;
    }
    parseBuffer();
}

void JetsonClient::parseBuffer()
{
    // TCP 스트림 청크로부터 완전한 패킷 단위 조립 및 프레임 언패킹
    while (m_rxBuffer.size() >= static_cast<int>(Config::HEADER_SIZE)) {
        const uchar* buf = reinterpret_cast<const uchar*>(m_rxBuffer.constData());

        // 8B 빅엔디안 헤더 파싱: [img_len(uint32, 4B)][json_len(uint32, 4B)]
        const quint32 imgSize = qFromBigEndian<quint32>(buf);
        const quint32 jsonSize = qFromBigEndian<quint32>(buf + 4);

        // 비정상 패킷 헤더 인입 시 버퍼 리셋 (동기화 복구)
        if (imgSize > Config::MAX_IMAGE_SIZE || jsonSize > Config::MAX_JSON_SIZE) {
            qWarning() << "[TCP] 비정상 패킷 헤더 -> 버퍼 초기화";
            m_rxBuffer.clear();
            return;
        }

        const int totalPacketSize = static_cast<int>(Config::HEADER_SIZE + imgSize + jsonSize);
        if (m_rxBuffer.size() < totalPacketSize) {
            return; // TCP 조각화(Fragmentation)로 인한 미완성 프레임 도착 대기
        }

        // 1. JPEG 바이너리 디코딩
        if (imgSize > 0) {
            QPixmap pixmap;
            if (pixmap.loadFromData(buf + Config::HEADER_SIZE, imgSize, "JPG")) {
                emit sigFrameReceived(pixmap);
            }
        }

        // 2. 메타데이터 역직렬화 (불필요한 힙 복사를 방지하는 0-Copy 슬라이스 참조 전달)
        if (jsonSize > 0) {
            const char* jsonPtr = reinterpret_cast<const char*>(buf + Config::HEADER_SIZE + imgSize);
            processJsonMeta(QByteArray::fromRawData(jsonPtr, static_cast<int>(jsonSize)));
        }

        // 3. 처리 완료된 패킷 바이트 제거
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

    // 엣지 추론 시점 타임스탬프와 UI 수신 시점 간의 종단 지연(Latency) 계산
    const double nowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const double latencyMs = qMax(0.0, (nowSec - meta.timestamp) * 1000.0);

    emit sigMetadataReceived(meta);
    emit sigTelemetryUpdated(meta.fps, meta.inferMs, latencyMs);
}