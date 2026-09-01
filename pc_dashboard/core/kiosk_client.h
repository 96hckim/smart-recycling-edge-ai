#ifndef KIOSK_CLIENT_H
#define KIOSK_CLIENT_H

#include <QObject>

class kiosk_client : public QObject
{
    Q_OBJECT
public:
    explicit kiosk_client(QObject *parent = nullptr);

signals:
};

#endif // KIOSK_CLIENT_H
