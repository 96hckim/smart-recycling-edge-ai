#ifndef JETSON_CLIENT_H
#define JETSON_CLIENT_H

#include <QObject>

class jetson_client : public QObject
{
    Q_OBJECT
public:
    explicit jetson_client(QObject *parent = nullptr);

signals:
};

#endif // JETSON_CLIENT_H
