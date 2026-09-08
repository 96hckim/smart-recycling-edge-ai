/**
 * Jetson Orin Nano TCP 비전 스트림 및 하드웨어 텔레메트리 수신 클라이언트 헤더.
 */
#pragma once
#ifndef JETSON_CLIENT_H
#define JETSON_CLIENT_H

#include "app_config.h"
#include <QByteArray>
#include <QObject>
#include <QPixmap>
#include <QTcpSocket>
#include <QTimer>

class JetsonClient : public QObject {
    Q_OBJECT

public:
    explicit JetsonClient(const QString& host = Config::DEFAULT_JETSON_IP,
        quint16 port = Config::JETSON_PORT,
        QObject* parent = nullptr);
    ~JetsonClient() override;

    // Jetson TCP 서버 연결 시도 (Non-blocking)
    void connectToJetson();
    // 소켓 세션 명시적 해제 및 재연결 타이머 중지
    void disconnectFromJetson();
    bool isConnected() const;

signals:
    void sigConnectionChanged(bool connected);
    void sigFrameReceived(const QPixmap& pixmap);
    void sigMetadataReceived(const FrameMetadata& meta);
    void sigTelemetryUpdated(double fps, double inferMs);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimeout();

private:
    // TCP 스트림 청크 누적 버퍼 검사 및 패킷 단위 언패킹 루프
    void parseBuffer();
    // JSON 메타데이터 역직렬화 및 종단 간 네트워크 지연(Latency) 계산
    void processJsonMeta(const QByteArray& jsonData);

    QTcpSocket* m_socket { nullptr };
    QTimer* m_reconnectTimer { nullptr };

    QString m_host;
    quint16 m_port;
    QByteArray m_rxBuffer;
};

#endif // JETSON_CLIENT_H