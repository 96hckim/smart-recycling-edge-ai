/**
 * 중앙 관제 백엔드 연동 WebSocket 인증 이벤트 및 REST API 결과 전송 클라이언트 헤더.
 */
#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

// 품목별 투입 수량 모델
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

    // 키오스크 전용 채널 WebSocket 연결
    void connectToKioskSocket();
    void disconnectSocket();
    bool isConnected() const;

    // 배출 완료 집계 데이터 백엔드 REST API 전송 (POST /api/recycle/submit)
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