#include "mqttclient.h"
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QStringDecoder>
#include <QTextStream>
#include <QDateTime>

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

void MqttClient::logToFile(const QString &message)
{
    QFile logFile("mqtt_debug.log");
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << message << "\n";
        logFile.close();
    }

    // 同时输出到qDebug，方便有Qt环境时查看
    qDebug() << message;
}


void MqttClient::connectToHost(const MqttConnectionConfig &config)
{
    // 记录开始连接
    logToFile("========== 开始MQTT连接 ==========");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    logToFile("主机: " + config.host);
    logToFile("端口: " + QString::number(config.port));
    logToFile("客户端ID: " + config.clientId);
    logToFile("用户名: " + config.username);
    logToFile("使用TLS: " + QString(config.useTLS ? "是" : "否"));
    logToFile("Clean Session: " + QString(config.cleanSession ? "是" : "否"));
    logToFile("Keep Alive: " + QString::number(config.keepAlive));

    // SSL支持检查
    logToFile("SSL支持: " + QString(QSslSocket::supportsSsl() ? "是" : "否"));
    logToFile("OpenSSL版本: " + QSslSocket::sslLibraryVersionString());
    logToFile("OpenSSL构建版本: " + QSslSocket::sslLibraryBuildVersionString());

    // 保存配置
    m_config = config;

    // 如果已连接，先断开
    if (m_client->state() != QMqttClient::Disconnected) {
        logToFile("当前已连接，先断开旧连接");
        m_client->disconnectFromHost();
        // 等待断开完成
        QThread::msleep(500);
    }

    // 设置基本连接参数
    m_client->setHostname(config.host);
    m_client->setPort(static_cast<quint16>(config.port));
    m_client->setClientId(config.clientId);
    m_client->setUsername(config.username);
    m_client->setPassword(config.password);
    m_client->setCleanSession(config.cleanSession);
    m_client->setKeepAlive(static_cast<quint16>(config.keepAlive));

    logToFile("基本参数设置完成");

    if (config.useTLS) {
        logToFile("===== TLS配置开始 =====");

        // 检查证书文件
        logToFile("CA证书路径: " + config.caCertPath);
        logToFile("客户端证书路径: " + config.clientCertPath);
        logToFile("客户端密钥路径: " + config.clientKeyPath);

        // 检查文件是否存在
        bool caExists = QFile::exists(config.caCertPath);
        bool certExists = QFile::exists(config.clientCertPath);
        bool keyExists = QFile::exists(config.clientKeyPath);

        logToFile("CA证书文件存在: " + QString(caExists ? "是" : "否"));
        logToFile("客户端证书文件存在: " + QString(certExists ? "是" : "否"));
        logToFile("客户端密钥文件存在: " + QString(keyExists ? "是" : "否"));

        // 创建SSL配置
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        logToFile("默认SSL配置获取成功");

        // 设置TLS协议
        sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
        logToFile("TLS协议设置为: TlsV1_2OrLater");

        // 加载CA证书
        QList<QSslCertificate> caCerts;

        if (!config.caCertPath.isEmpty() && caExists) {
            QFile caFile(config.caCertPath);
            if (caFile.open(QIODevice::ReadOnly)) {
                QByteArray caData = caFile.readAll();
                caFile.close();

                // 尝试多种方式加载CA证书
                caCerts = QSslCertificate::fromData(caData, QSsl::Pem);
                if (caCerts.isEmpty()) {
                    caCerts = QSslCertificate::fromData(caData, QSsl::Der);
                }

                logToFile("从文件加载的CA证书数量: " + QString::number(caCerts.size()));

                // 记录证书详情
                for (int i = 0; i < caCerts.size(); ++i) {
                    const QSslCertificate &cert = caCerts.at(i);
                    logToFile(QString("CA证书[%1] - 颁发者: %2").arg(i).arg(QString(cert.issuerDisplayName())));
                    logToFile(QString("CA证书[%1] - 使用者: %2").arg(i).arg(QString(cert.subjectDisplayName())));
                    logToFile(QString("CA证书[%1] - 有效期: %2 至 %3")
                                  .arg(i)
                                  .arg(cert.effectiveDate().toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(cert.expiryDate().toString("yyyy-MM-dd hh:mm:ss")));
                }
            } else {
                logToFile("错误: 无法打开CA证书文件: " + caFile.errorString());
            }
        }

        // 添加系统CA证书
        QList<QSslCertificate> systemCerts = QSslConfiguration::systemCaCertificates();
        logToFile("系统CA证书数量: " + QString::number(systemCerts.size()));

        if (!systemCerts.isEmpty()) {
            caCerts.append(systemCerts);
            logToFile("合并后CA证书总数: " + QString::number(caCerts.size()));
        }

        sslConfig.setCaCertificates(caCerts);
        logToFile("CA证书设置完成");

        // 加载客户端证书（双向认证）
        if (!config.clientCertPath.isEmpty() && !config.clientKeyPath.isEmpty() &&
            certExists && keyExists) {

            logToFile("===== 加载客户端证书 =====");

            // 加载客户端证书
            QFile certFile(config.clientCertPath);
            if (certFile.open(QIODevice::ReadOnly)) {
                QByteArray certData = certFile.readAll();
                certFile.close();

                // 尝试多种格式加载证书
                QSslCertificate clientCert = QSslCertificate(certData, QSsl::Pem);
                if (clientCert.isNull()) {
                    clientCert = QSslCertificate(certData, QSsl::Der);
                }

                if (!clientCert.isNull()) {
                    sslConfig.setLocalCertificate(clientCert);
                    logToFile("客户端证书加载成功");
                    logToFile("客户端证书 - 颁发者: " + QString(clientCert.issuerDisplayName()));
                    logToFile("客户端证书 - 使用者: " + QString(clientCert.subjectDisplayName()));
                    logToFile("客户端证书 - 有效期: " + clientCert.effectiveDate().toString("yyyy-MM-dd hh:mm:ss") +
                              " 至 " + clientCert.expiryDate().toString("yyyy-MM-dd hh:mm:ss"));
                } else {
                    logToFile("错误: 无法加载客户端证书");
                }
            }

            // 加载私钥
            QFile keyFile(config.clientKeyPath);
            if (keyFile.open(QIODevice::ReadOnly)) {
                QByteArray keyData = keyFile.readAll();
                keyFile.close();

                // 尝试RSA和EC格式
                QSslKey privateKey(keyData, QSsl::Rsa, QSsl::Pem);
                if (privateKey.isNull()) {
                    privateKey = QSslKey(keyData, QSsl::Ec, QSsl::Pem);
                }
                if (privateKey.isNull()) {
                    privateKey = QSslKey(keyData, QSsl::Rsa, QSsl::Der);
                }
                if (privateKey.isNull()) {
                    privateKey = QSslKey(keyData, QSsl::Ec, QSsl::Der);
                }

                if (!privateKey.isNull()) {
                    sslConfig.setPrivateKey(privateKey);
                    logToFile("私钥加载成功，算法: " + QString(privateKey.algorithm() == QSsl::Rsa ? "RSA" : "EC"));
                } else {
                    logToFile("错误: 无法加载私钥");
                    emit errorOccurred("TLS: 无法加载私钥");
                }
            }
        }

        // 设置SSL选项
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
        logToFile("SSL验证模式: VerifyPeer");

        // 注意：移除了sslErrors信号的连接，因为您的Qt版本不支持

        // 尝试通过errorChanged信号来捕获SSL相关错误
        logToFile("===== TLS配置完成，开始加密连接 =====");
        m_client->connectToHostEncrypted(sslConfig);

        // 添加一个单次定时器来检查连接状态
        QTimer::singleShot(5000, this, [this]() {
            if (m_client->state() != QMqttClient::Connected) {
                logToFile("警告: 连接超时（5秒后仍未连接成功）");
                logToFile("当前连接状态: " + QString::number(m_client->state()));
            }
        });

    } else {
        logToFile("===== 开始非加密连接 =====");
        m_client->connectToHost();
    }

    logToFile("connectToHost函数执行完成");
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
    return m_connected;
}

void MqttClient::onConnected()
{
    m_connected = true;
    emit connected();
}

void MqttClient::onDisconnected()
{
    m_connected = false;
    emit disconnected();
}

void MqttClient::onMessageReceived(const QMqttMessage &message)
{
    QString text;
    const QByteArray &payload = message.payload();

    // Try to decode as UTF-8; if the payload has invalid bytes, show as HEX
    auto decoder = QStringDecoder(QStringConverter::Utf8);
    text = decoder(payload);
    if (decoder.hasError()) {
        text = "HEX: " + QString::fromLatin1(payload.toHex(' ')).toUpper();
    }

    emit messageReceived(message.topic().name(), text, message.retain());
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorMsg = mqttErrorString(error);
    logToFile("===== 错误发生 =====");
    logToFile("错误码: " + QString::number(error));
    logToFile("错误信息: " + errorMsg);

    // 添加更多调试信息
    logToFile("当前连接状态: " + QString::number(m_client->state()));
    logToFile("主机: " + m_client->hostname());
    logToFile("端口: " + QString::number(m_client->port()));

    if (error != QMqttClient::NoError) {
        emit errorOccurred(errorMsg);
    }
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
