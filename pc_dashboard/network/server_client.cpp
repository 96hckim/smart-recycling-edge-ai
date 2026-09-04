#include "server_client.h"
#include "app_config.h"
#include <QDebug>

using namespace Config::Backend;

ServerClient::ServerClient(int binId, const QString& serverHost, int serverPort, QObject* parent)
    : QObject(parent)
    , m_binId(binId)
    , m_serverHost(serverHost)
    , m_serverPort(serverPort)
    , m_reconnectTimer(new QTimer(this))
{
    connect(&m_webSocket, &QWebSocket::connected, this, &ServerClient::onSocketConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &ServerClient::onSocketDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &ServerClient::onSocketMessageReceived);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
        this, &ServerClient::onSocketError);

    connect(&m_httpManager, &QNetworkAccessManager::finished, this, &ServerClient::onSubmitReplyFinished);

    m_reconnectTimer->setInterval(Config::AUTO_RECONNECT_INTERVAL_MS);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ServerClient::onReconnectTimeout);
}

ServerClient::~ServerClient()
{
    disconnectSocket();
}

void ServerClient::connectToKioskSocket()
{
    if (m_webSocket.state() == QAbstractSocket::UnconnectedState) {
        QString wsUrl = WS_URL_FMT.arg(m_serverHost).arg(m_serverPort).arg(m_binId);
        qDebug() << "[ServerClient] Connecting to WS:" << wsUrl;
        m_webSocket.open(QUrl(wsUrl));
    }
}

void ServerClient::disconnectSocket()
{
    m_reconnectTimer->stop();
    if (m_webSocket.isValid()) {
        m_webSocket.close();
    }
}

bool ServerClient::isConnected() const
{
    return (m_webSocket.state() == QAbstractSocket::ConnectedState);
}

void ServerClient::onSocketConnected()
{
    qDebug() << "[ServerClient] WebSocket Connected for Bin ID:" << m_binId;
    m_reconnectTimer->stop(); // 연결 성공 시 재연결 타이머 중지
}

void ServerClient::onSocketDisconnected()
{
    qWarning() << "[ServerClient] WebSocket Disconnected. 재연결 시도 대기 중...";
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(); // 연결 끊김 감지 시 자동 재연결 시작
    }
}

void ServerClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errStr = m_webSocket.errorString();
    qWarning() << "[ServerClient] Socket Error:" << errStr;
    emit networkErrorOccurred(errStr);

    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(); // 소켓 오류 시에도 재연결 시작
    }
}

void ServerClient::onReconnectTimeout()
{
    if (m_webSocket.state() == QAbstractSocket::UnconnectedState) {
        connectToKioskSocket();
    }
}

void ServerClient::onSocketMessageReceived(const QString& message)
{
    qDebug() << "[ServerClient] WS Message Received:" << message;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[ServerClient] Invalid JSON payload:" << parseErr.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString eventType = obj[Key::EVENT].toString();

    if (eventType == Event::USER_AUTHENTICATED) {
        int userId = obj[Key::USER_ID].toInt();
        QString name = obj[Key::NAME].toString(Config::Demo::MEMBER_USER_ID);
        QString phone = obj[Key::PHONE].toString();
        int points = obj[Key::POINTS].toInt();

        emit userAuthenticated(userId, name, phone, points);
    } else {
        qDebug() << "[ServerClient] Unhandled WS Event:" << eventType;
    }
}

void ServerClient::submitRecycleResult(int userId, const RecycleCounts& counts, double carbonSaved, int earnedPoints)
{
    QUrl url(API_SUBMIT_PATH.arg(m_serverHost).arg(m_serverPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body[Key::BIN_ID] = m_binId;
    body[Key::USER_ID] = (userId > 0) ? QJsonValue(userId) : QJsonValue(QJsonValue::Null);

    body[Key::PAPER_COUNT] = counts.paper;
    body[Key::CAN_COUNT] = counts.can;
    body[Key::PET_COUNT] = counts.pet;
    body[Key::VINYL_COUNT] = counts.vinyl;

    body[Key::CARBON_SAVED] = carbonSaved;
    body[Key::EARNED_PTS] = earnedPoints;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    qDebug() << "[ServerClient] POST /api/recycle/submit:" << postData;
    m_httpManager.post(request, postData);
}

void ServerClient::onSubmitReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString errStr = reply->errorString();
        qWarning() << "[ServerClient] REST Submit Error:" << errStr;
        emit networkErrorOccurred(errStr);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    int logId = obj[Key::LOG_ID].toInt();
    int totalPoints = obj[Key::TOTAL_POINTS].toInt();

    emit submitCompleted(logId, totalPoints);
}