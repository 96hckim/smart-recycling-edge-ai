#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QWebSocket>

// 순서 표준화: 종이 -> 캔 -> 페트 -> 비닐
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

    // 키오스크 대기 화면 진입 시 웹소켓 연결
    void connectToKioskSocket();
    void disconnectSocket();

    // 분리수거 투입 완료 REST 요청
    void submitRecycleResult(int userId, const RecycleCounts& counts, double carbonSaved, int earnedPoints);

signals:
    // 모바일이 QR을 찍고 로그인했을 때 방출 (화면 전환 트리거)
    void userAuthenticated(int userId, const QString& phone, int currentPoints);

    // REST 투입 정산 성공 시 방출
    void submitCompleted(int logId, int totalPoints);

    // 네트워크 오류 알림
    void networkErrorOccurred(const QString& errorMessage);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketMessageReceived(const QString& message);
    void onSocketError(QAbstractSocket::SocketError error);
    void onSubmitReplyFinished(QNetworkReply* reply);

private:
    int m_binId;
    QString m_serverHost;
    int m_serverPort;

    QWebSocket m_webSocket;
    QNetworkAccessManager m_httpManager;
};