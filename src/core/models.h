#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

struct MqttConnectionConfig {
    int id;
    QString name;
    QString host;
    int port;
    QString username;
    QString password;
    QString clientId;
    bool useTLS;
    QString caCertPath;
    QString clientCertPath;
    QString clientKeyPath;
    bool cleanSession;
    int keepAlive;

    MqttConnectionConfig()
        : id(-1), host("localhost"), port(1883),
          useTLS(false), cleanSession(true), keepAlive(60) {}
};

struct CommandConfig {
    int id;
    QString name;
    QString topic;
    QString payload;
    int qos;
    bool retain;
    bool loopEnabled;
    int loopIntervalMs;
    int connectionId;

    CommandConfig()
        : id(-1), qos(0), retain(false),
          loopEnabled(false), loopIntervalMs(1000), connectionId(-1) {}
};

struct ScriptConfig {
    int id;
    QString name;
    bool enabled;
    QString triggerTopic;
    QString triggerCondition; // "any", "contains", "equals", "startsWith", "endsWith", "regex"
    QString triggerValue;
    QString responseTopic;
    QString responsePayload;
    int responseQos;
    bool responseRetain;
    int delayMs;
    int connectionId;

    ScriptConfig()
        : id(-1), enabled(true), triggerCondition("any"),
          responseQos(0), responseRetain(false), delayMs(0), connectionId(-1) {}
};

struct SubscriptionConfig {
    int id;
    int connectionId;
    QString topic;
    int qos;

    SubscriptionConfig()
        : id(-1), connectionId(-1), qos(0) {}
};

struct MessageRecord {
    int id;
    int connectionId;
    QString topic;
    QString payload;
    bool outgoing;
    bool retained;
    QDateTime timestamp;

    MessageRecord()
        : id(-1), connectionId(-1), outgoing(false), retained(false) {}
};

Q_DECLARE_METATYPE(MqttConnectionConfig)

#endif // MODELS_H
