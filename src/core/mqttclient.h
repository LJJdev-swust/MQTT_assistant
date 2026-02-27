#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QMqttClient>
#include <QMqttMessage>
#include <QMqttTopicFilter>
#include <QMqttTopicName>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QAtomicInt>
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

    // Thread-safe: uses atomic flag updated by onConnected/onDisconnected
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
    QAtomicInt   m_connected{0}; // 1 = connected, 0 = not connected
    QString mqttErrorString(QMqttClient::ClientError error) const;
};

#endif // MQTTCLIENT_H
