#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QMqttClient>
#include <QMqttTopicFilter>
#include <QMqttTopicName>
#include <QSslSocket>
#include <QSslConfiguration>
#include "models.h"

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    void connectToHost(const MqttConnectionConfig &config);
    void disconnectFromHost();
    void publish(const QString &topic, const QString &payload, int qos = 0, bool retain = false);
    void subscribe(const QString &topic, int qos = 0);
    void unsubscribe(const QString &topic);

    bool isConnected() const;
    MqttConnectionConfig currentConfig() const { return m_config; }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &topic, const QString &payload);
    void errorOccurred(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QMqttMessage &message);
    void onErrorChanged(QMqttClient::ClientError error);

private:
    QMqttClient *m_client;
    MqttConnectionConfig m_config;
    QString mqttErrorString(QMqttClient::ClientError error) const;
};

#endif // MQTTCLIENT_H
