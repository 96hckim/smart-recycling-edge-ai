#pragma once
#ifndef JETSON_CLIENT_H
#define JETSON_CLIENT_H

#include "app_config.h"
#include <QByteArray>
#include <QObject>
#include <QPixmap>
#include <QTcpSocket>
#include <QTimer>

class JetsonClient : public QObject
{
    Q_OBJECT

public:
    explicit JetsonClient(const QString &host = Config::DEFAULT_JETSON_IP,
                          quint16 port = Config::JETSON_PORT,
                          QObject *parent = nullptr);
    ~JetsonClient() override;

    void connectToJetson();
    void disconnectFromJetson();
    bool isConnected() const;

signals:
    void sigConnectionChanged(bool connected);
    void sigFrameReceived(const QPixmap &pixmap);
    void sigMetadataReceived(const FrameMetadata &meta);
    void sigTelemetryUpdated(double fps, double inferMs, double latencyMs);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimeout();

private:
    void parseBuffer();
    void processJsonMeta(const QByteArray &jsonData);

    QTcpSocket *m_socket{nullptr};
    QTimer *m_reconnectTimer{nullptr};

    QString m_host;
    quint16 m_port;
    QByteArray m_rxBuffer;
};

#endif // JETSON_CLIENT_H