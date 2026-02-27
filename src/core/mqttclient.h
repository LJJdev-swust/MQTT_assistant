#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QMqttClient>
#include <QMqttTopicFilter>
#include <QMqttTopicName>
#include <QMqttMessage>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <QAtomicInt>
#include <QTimer>
#include <QOverload>
#include "models.h"

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    Q_INVOKABLE void connectToHost(const MqttConnectionConfig &config);
    Q_INVOKABLE void disconnectFromHost();
    Q_INVOKABLE void publish(const QString &topic, const QString &payload, int qos = 0, bool retain = false);
    Q_INVOKABLE void subscribe(const QString &topic, int qos = 0);
    Q_INVOKABLE void unsubscribe(const QString &topic);

    bool isConnected() const;
    MqttConnectionConfig currentConfig() const { return m_config; }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &topic, const QString &payload, bool retained);
    void errorOccurred(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QMqttMessage &message);
    void onErrorChanged(QMqttClient::ClientError error);

private:
    QMqttClient  *m_client;
    MqttConnectionConfig m_config;
    bool m_connected = false;
    QString mqttErrorString(QMqttClient::ClientError error) const;
    void logToFile(const QString &message);

};

#endif // MQTTCLIENT_H
