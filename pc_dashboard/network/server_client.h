#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer> // 추가
#include <QUrl>
#include <QWebSocket>

struct RecycleCounts {
    int paper = 0;
    int can = 0;
    int pet = 0;
    int vinyl = 0;
};

class ServerClient : public QObject {
    Q_OBJECT

public:
    explicit ServerClient(int binId, const QString& serverHost, int serverPort, QObject* parent = nullptr);
    ~ServerClient() override;

    void connectToKioskSocket();
    void disconnectSocket();
    bool isConnected() const;

    void submitRecycleResult(int userId, const RecycleCounts& counts, double carbonSaved, int earnedPoints);

signals:
    void userAuthenticated(int userId, const QString& name, const QString& phone, int currentPoints);
    void submitCompleted(int logId, int totalPoints);
    void networkErrorOccurred(const QString& errorMessage);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketMessageReceived(const QString& message);
    void onSocketError(QAbstractSocket::SocketError error);
    void onSubmitReplyFinished(QNetworkReply* reply);
    void onReconnectTimeout();

private:
    int m_binId;
    QString m_serverHost;
    int m_serverPort;

    QWebSocket m_webSocket;
    QNetworkAccessManager m_httpManager;
    QTimer* m_reconnectTimer { nullptr };
};