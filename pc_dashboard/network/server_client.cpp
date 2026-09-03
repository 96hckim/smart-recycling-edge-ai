#include "server_client.h"
#include <QDebug>

ServerClient::ServerClient(int binId, const QString& serverHost, int serverPort, QObject* parent)
    : QObject(parent)
    , m_binId(binId)
    , m_serverHost(serverHost)
    , m_serverPort(serverPort)
{
    // QWebSocket 시그널 바인딩
    connect(&m_webSocket, &QWebSocket::connected, this, &ServerClient::onSocketConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &ServerClient::onSocketDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &ServerClient::onSocketMessageReceived);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
        this, &ServerClient::onSocketError);

    // REST 응답 처리
    connect(&m_httpManager, &QNetworkAccessManager::finished, this, &ServerClient::onSubmitReplyFinished);
}

ServerClient::~ServerClient()
{
    m_webSocket.close();
}

void ServerClient::connectToKioskSocket()
{
    QString wsUrl = QString("ws://%1:%2/ws/kiosk/%3/kiosk")
                        .arg(m_serverHost)
                        .arg(m_serverPort)
                        .arg(m_binId);

    qDebug() << "[ServerClient] Connecting to WS:" << wsUrl;
    m_webSocket.open(QUrl(wsUrl));
}

void ServerClient::disconnectSocket()
{
    if (m_webSocket.isValid()) {
        m_webSocket.close();
    }
}

void ServerClient::onSocketConnected()
{
    qDebug() << "[ServerClient] WebSocket Connected for Bin ID:" << m_binId;
}

void ServerClient::onSocketDisconnected()
{
    qDebug() << "[ServerClient] WebSocket Disconnected.";
}

void ServerClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errStr = m_webSocket.errorString();
    qWarning() << "[ServerClient] Socket Error:" << errStr;
    emit networkErrorOccurred(errStr);
}

void ServerClient::onSocketMessageReceived(const QString& message)
{
    qDebug() << "[ServerClient] WS Message Received:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    QString eventType = obj["event"].toString();

    // 서버가 보낸 USER_AUTHENTICATED 이벤트 파싱
    if (eventType == "USER_AUTHENTICATED") {
        int userId = obj["user_id"].toInt();
        QString phone = obj["phone"].toString();
        int points = obj["points"].toInt();

        emit userAuthenticated(userId, phone, points);
    }
}

void ServerClient::submitRecycleResult(int userId, const RecycleCounts& counts, double carbonSaved, int earnedPoints)
{
    QUrl url(QString("http://%1:%2/api/recycle/submit").arg(m_serverHost).arg(m_serverPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["bin_id"] = m_binId;
    if (userId > 0) {
        body["user_id"] = userId;
    } else {
        body["user_id"] = QJsonValue::Null;
    }

    // 순서: 종이 -> 캔 -> 페트 -> 비닐
    body["paper_count"] = counts.paper;
    body["can_count"] = counts.can;
    body["pet_count"] = counts.pet;
    body["vinyl_count"] = counts.vinyl;

    body["carbon_saved_g"] = carbonSaved;
    body["earned_points"] = earnedPoints;

    QJsonDocument doc(body);
    QByteArray postData = doc.toJson();

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

    QByteArray respData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(respData);
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    int logId = obj["log_id"].toInt();
    int totalPoints = obj["total_points"].toInt();

    emit submitCompleted(logId, totalPoints);
}