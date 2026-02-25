#include "mqttclient.h"
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QStringDecoder>

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
{
    connect(m_client, &QMqttClient::connected,    this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttClient::onErrorChanged);
}

MqttClient::~MqttClient()
{
    if (m_client->state() != QMqttClient::Disconnected)
        m_client->disconnectFromHost();
}

void MqttClient::connectToHost(const MqttConnectionConfig &config)
{
    m_config = config;

    if (m_client->state() != QMqttClient::Disconnected)
        m_client->disconnectFromHost();

    m_client->setHostname(config.host);
    m_client->setPort(static_cast<quint16>(config.port));
    m_client->setClientId(config.clientId);
    m_client->setUsername(config.username);
    m_client->setPassword(config.password);
    m_client->setCleanSession(config.cleanSession);
    m_client->setKeepAlive(static_cast<quint16>(config.keepAlive));

    if (config.useTLS) {
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

        if (!config.caCertPath.isEmpty()) {
            QList<QSslCertificate> caCerts = QSslCertificate::fromPath(config.caCertPath);
            if (!caCerts.isEmpty())
                sslConfig.setCaCertificates(caCerts);
        }
        if (!config.clientCertPath.isEmpty() && !config.clientKeyPath.isEmpty()) {
            QFile certFile(config.clientCertPath);
            QFile keyFile(config.clientKeyPath);
            if (certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly)) {
                QSslCertificate cert(&certFile, QSsl::Pem);
                // Try RSA first, then EC for broader key-type support
                QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
                if (key.isNull()) {
                    keyFile.seek(0);
                    key = QSslKey(&keyFile, QSsl::Ec, QSsl::Pem);
                }
                sslConfig.setLocalCertificate(cert);
                sslConfig.setPrivateKey(key);
                if (key.isNull()) {
                    emit errorOccurred("TLS: failed to load private key (tried RSA and EC)");
                }
            }
        }
        m_client->connectToHostEncrypted(sslConfig);
    } else {
        m_client->connectToHost();
    }
}

void MqttClient::disconnectFromHost()
{
    m_client->disconnectFromHost();
}

void MqttClient::publish(const QString &topic, const QString &payload, int qos, bool retain)
{
    if (m_client->state() != QMqttClient::Connected) {
        emit errorOccurred("Not connected");
        return;
    }
    QMqttTopicName topicName(topic);
    m_client->publish(topicName, payload.toUtf8(), qos, retain);
}

void MqttClient::subscribe(const QString &topic, int qos)
{
    if (m_client->state() != QMqttClient::Connected) {
        emit errorOccurred("Not connected");
        return;
    }
    QMqttTopicFilter filter(topic);
    m_client->subscribe(filter, static_cast<quint8>(qos));
}

void MqttClient::unsubscribe(const QString &topic)
{
    QMqttTopicFilter filter(topic);
    m_client->unsubscribe(filter);
}

bool MqttClient::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

void MqttClient::onConnected()
{
    emit connected();
}

void MqttClient::onDisconnected()
{
    emit disconnected();
}

void MqttClient::onMessageReceived(const QByteArray &payload, const QMqttTopicName &topic)
{
    const QByteArray &bytes = message.payload();
    QString text;
    // Try to decode as UTF-8; if the payload has invalid bytes, show as HEX
    auto decoder = QStringDecoder(QStringConverter::Utf8);
    text = decoder(bytes);
    if (decoder.hasError()) {
        text = "HEX: " + QString::fromLatin1(bytes.toHex(' ')).toUpper();
    }
    emit messageReceived(message.topic().name(), text);
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    if (error != QMqttClient::NoError)
        emit errorOccurred(mqttErrorString(error));
}

QString MqttClient::mqttErrorString(QMqttClient::ClientError error) const
{
    switch (error) {
    case QMqttClient::InvalidProtocolVersion: return "Invalid protocol version";
    case QMqttClient::IdRejected:             return "Client ID rejected";
    case QMqttClient::ServerUnavailable:      return "Server unavailable";
    case QMqttClient::BadUsernameOrPassword:  return "Bad username or password";
    case QMqttClient::NotAuthorized:          return "Not authorized";
    case QMqttClient::TransportInvalid:       return "Transport invalid";
    case QMqttClient::ProtocolViolation:      return "Protocol violation";
    case QMqttClient::UnknownError:           return "Unknown error";
    default:                                  return "Error " + QString::number(error);
    }
}
