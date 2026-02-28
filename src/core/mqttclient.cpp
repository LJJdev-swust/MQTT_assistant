#include "mqttclient.h"
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QStringDecoder>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>

// ─── 文件内日志辅助 ───────────────────────────────────────────
// 所有旧的 logToFile() 调用均通过此处路由到统一的 Logger 系统。
// 根据消息内容自动选择合适的日志级别。
static void logToFile(const QString &message)
{
    if (message.startsWith("【错误") || message.startsWith("错误:")) {
        Logger::error("MQTT", message);
    } else if (message.startsWith("警告") || message.startsWith("【警告")) {
        Logger::warning("MQTT", message);
    } else if (message.startsWith("====") ||
               (message.startsWith("========") && message.endsWith("========"))) {
        Logger::instance().separator();
    } else if (message.startsWith("=======")) {
        // 章节标题行：去除等号后作为分隔标题
        QString title = message;
        title.remove('=').simplified();
        if (!title.isEmpty())
            Logger::instance().separator(title);
        else
            Logger::instance().separator();
    } else {
        Logger::debug("MQTT", message);
    }
}
// ─────────────────────────────────────────────────────────────

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_client(nullptr)
{
}

void MqttClient::init()
{
    m_client = new QMqttClient(this);
    logToFile("========================================");
    logToFile("========== MqttClient 初始化 ==========");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("应用程序路径: " + QCoreApplication::applicationFilePath());
    logToFile("当前工作目录: " + QDir::currentPath());
    logToFile("临时目录: " + QDir::tempPath());

    connect(m_client, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(m_client, SIGNAL(messageReceived(QByteArray,QMqttTopicName)),
            this, SLOT(onMessageReceived(QByteArray,QMqttTopicName)));
    connect(m_client, SIGNAL(errorChanged(QMqttClient::ClientError)),
            this, SLOT(onErrorChanged(QMqttClient::ClientError)));

    logToFile("信号连接完成");
    logToFile("========================================");

}

MqttClient::~MqttClient()
{
    logToFile("========================================");
    logToFile("========== MqttClient 析构 ==========");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));

    if (m_client && m_client->state() != QMqttClient::Disconnected)
        logToFile("正在断开连接...");
        m_client->disconnectFromHost();

    logToFile("析构完成");
    logToFile("========================================");
}

// ─── 二进制数据检测辅助 ──────────────────────────────────────────────────────
// 即使 UTF-8 解码成功，二进制载荷中仍可能含有大量控制字符，
// 在界面上显示为乱码。此函数通过检测控制字符比例来决定是否按 HEX 显示。
static bool looksLikeBinary(const QByteArray &data)
{
    if (data.isEmpty())
        return false;

    int ctrlCount = 0;
    for (unsigned char c : data) {
        // 允许：可见 ASCII(0x20-0x7E)、制表符(0x09)、换行(0x0A)、回车(0x0D)
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)
            ++ctrlCount;
        else if (c == 0x7F)
            ++ctrlCount;
    }

    // 若控制字符超过总字节数的 5% 则视为二进制
    return ctrlCount > 0 && (ctrlCount * 100 / data.size()) > 5;
}
// ─────────────────────────────────────────────────────────────────────────────

void MqttClient::connectToHost(const MqttConnectionConfig &config)
{
    if (!m_client) {
        emit errorOccurred("Client not initialized");
        return;
    }

    // 记录开始连接
    logToFile("========================================");
    logToFile("========== 开始MQTT连接 ==========");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("连接ID: " + QString::number(config.id));
    logToFile("连接名称: " + config.name);
    logToFile("主机: " + config.host);
    logToFile("端口: " + QString::number(config.port));
    logToFile("客户端ID: " + config.clientId);
    logToFile("用户名: " + config.username);
    logToFile("密码: " + QString(config.password.isEmpty() ? "空" : "已设置"));
    logToFile("使用TLS: " + QString(config.useTLS ? "是" : "否"));
    logToFile("Clean Session: " + QString(config.cleanSession ? "是" : "否"));
    logToFile("Keep Alive: " + QString::number(config.keepAlive));

    if (config.useTLS) {
        logToFile("CA证书路径: " + config.caCertPath);
        logToFile("客户端证书路径: " + config.clientCertPath);
        logToFile("客户端密钥路径: " + config.clientKeyPath);
    }

    // SSL支持检查
    logToFile("SSL支持: " + QString(QSslSocket::supportsSsl() ? "是" : "否"));
    logToFile("OpenSSL版本: " + QSslSocket::sslLibraryVersionString());
    logToFile("OpenSSL构建版本: " + QSslSocket::sslLibraryBuildVersionString());

    // 保存配置
    m_config = config;

    // 记录当前连接状态
    logToFile("当前客户端状态: " + QString::number(m_client->state()));
    logToFile("当前连接状态标志: " + QString(m_connected ? "true" : "false"));

    // 如果已连接，先断开
    if (m_client->state() != QMqttClient::Disconnected) {
        logToFile("当前已连接，先断开旧连接");
        m_client->disconnectFromHost();
        logToFile("等待断开完成...");
        QThread::msleep(500);
        logToFile("断开后状态: " + QString::number(m_client->state()));
    }

    // 设置基本连接参数
    logToFile("设置基本连接参数:");
    m_client->setHostname(config.host);
    logToFile("  - Hostname: " + config.host);

    m_client->setPort(static_cast<quint16>(config.port));
    logToFile("  - Port: " + QString::number(config.port));

    m_client->setClientId(config.clientId);
    logToFile("  - ClientId: " + config.clientId);

    m_client->setUsername(config.username);
    logToFile("  - Username: " + config.username);

    m_client->setPassword(config.password);
    logToFile("  - Password: " + QString(config.password.isEmpty() ? "空" : "已设置"));

    m_client->setCleanSession(config.cleanSession);
    logToFile("  - CleanSession: " + QString(config.cleanSession ? "true" : "false"));

    m_client->setKeepAlive(static_cast<quint16>(config.keepAlive));
    logToFile("  - KeepAlive: " + QString::number(config.keepAlive));

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
                logToFile("CA证书文件大小: " + QString::number(caData.size()) + " 字节");

                // 尝试多种方式加载CA证书
                caCerts = QSslCertificate::fromData(caData, QSsl::Pem);
                if (caCerts.isEmpty()) {
                    logToFile("PEM格式加载失败，尝试DER格式");
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
        } else {
            logToFile("CA证书路径为空或文件不存在，跳过加载");
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
                logToFile("客户端证书文件大小: " + QString::number(certData.size()) + " 字节");

                // 尝试多种格式加载证书
                QSslCertificate clientCert = QSslCertificate(certData, QSsl::Pem);
                if (clientCert.isNull()) {
                    logToFile("PEM格式加载失败，尝试DER格式");
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
            } else {
                logToFile("错误: 无法打开客户端证书文件: " + certFile.errorString());
            }

            // 加载私钥
            QFile keyFile(config.clientKeyPath);
            if (keyFile.open(QIODevice::ReadOnly)) {
                QByteArray keyData = keyFile.readAll();
                keyFile.close();
                logToFile("私钥文件大小: " + QString::number(keyData.size()) + " 字节");

                // 尝试RSA和EC格式
                QSslKey privateKey(keyData, QSsl::Rsa, QSsl::Pem);
                if (privateKey.isNull()) {
                    logToFile("RSA PEM格式失败，尝试EC PEM格式");
                    privateKey = QSslKey(keyData, QSsl::Ec, QSsl::Pem);
                }
                if (privateKey.isNull()) {
                    logToFile("PEM格式失败，尝试RSA DER格式");
                    privateKey = QSslKey(keyData, QSsl::Rsa, QSsl::Der);
                }
                if (privateKey.isNull()) {
                    logToFile("RSA DER格式失败，尝试EC DER格式");
                    privateKey = QSslKey(keyData, QSsl::Ec, QSsl::Der);
                }

                if (!privateKey.isNull()) {
                    sslConfig.setPrivateKey(privateKey);
                    logToFile("私钥加载成功，算法: " + QString(privateKey.algorithm() == QSsl::Rsa ? "RSA" : "EC"));
                    logToFile("私钥长度: " + QString::number(privateKey.length()));
                } else {
                    logToFile("错误: 无法加载私钥");
                    emit errorOccurred("TLS: 无法加载私钥");
                }
            } else {
                logToFile("错误: 无法打开私钥文件: " + keyFile.errorString());
            }
        } else {
            logToFile("客户端证书或密钥路径为空或文件不存在，跳过双向认证配置");
        }

        // 设置SSL选项
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
        logToFile("SSL验证模式: VerifyPeer");

        logToFile("===== TLS配置完成，开始加密连接 =====");
        m_client->connectToHostEncrypted(sslConfig);

        // 添加一个单次定时器来检查连接状态
        QTimer::singleShot(5000, this, [this]() {
            if (m_client->state() != QMqttClient::Connected) {
                logToFile("警告: 连接超时（5秒后仍未连接成功）");
                logToFile("当前连接状态: " + QString::number(m_client->state()));
                logToFile("当前连接状态标志: " + QString(m_connected ? "true" : "false"));
            }
        });

    } else {
        logToFile("===== 开始非加密连接 =====");
        m_client->connectToHost();
    }

    logToFile("connectToHost函数执行完成");
    logToFile("========================================");
}

void MqttClient::disconnectFromHost()
{
    if (!m_client) return;
    logToFile("========================================");
    logToFile("disconnectFromHost 被调用");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("当前连接状态: " + QString::number(m_client->state()));

    m_client->disconnectFromHost();

    logToFile("断开连接命令已发送");
    logToFile("========================================");
}

void MqttClient::publish(const QString &topic, const QString &payload, int qos, bool retain)
{
    logToFile("========================================");
    logToFile("【发布消息】");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("主题: " + topic);
    logToFile("内容: " + payload);
    logToFile("QoS: " + QString::number(qos));
    logToFile("Retain: " + QString(retain ? "true" : "false"));

    // 添加调用堆栈信息（如果有）
    logToFile("调用来源: " + QString("请检查代码中谁调用了publish"));

    if (!m_client || m_client->state() != QMqttClient::Connected) {
        logToFile("【错误】发布消息时未连接!");
        emit errorOccurred("Not connected");
        logToFile("========================================");
        return;
    }

    QMqttTopicName topicName(topic);
    m_client->publish(topicName, payload.toUtf8(), qos, retain);
    logToFile("【发布消息】完成");
    logToFile("========================================");
}

void MqttClient::subscribe(const QString &topic, int qos)
{
    logToFile("========================================");
    logToFile("【订阅主题】");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("主题: " + topic);
    logToFile("QoS: " + QString::number(qos));

    if (!m_client || m_client->state() != QMqttClient::Connected) {
        logToFile("【错误】订阅时未连接!");
        emit errorOccurred("Not connected");
        logToFile("========================================");
        return;
    }

    QMqttTopicFilter filter(topic);
    auto subscription = m_client->subscribe(filter, static_cast<quint8>(qos));

    if (subscription) {
        logToFile("【订阅主题】成功");

        // 重要：检查订阅成功后是否有自动发布逻辑
        if (topic.contains("ress/query/")) {
            logToFile("【警告】订阅了query主题，检查是否有自动发布代码");
        }
    } else {
        logToFile("【订阅主题】失败!");
    }
    logToFile("========================================");
}

void MqttClient::unsubscribe(const QString &topic)
{
    if (!m_client) return;
    logToFile("========================================");
    logToFile("【取消订阅】");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("主题: " + topic);

    QMqttTopicFilter filter(topic);
    m_client->unsubscribe(filter);

    logToFile("【取消订阅】完成");
    logToFile("========================================");
}

bool MqttClient::isConnected() const
{
    return m_connected;
}

void MqttClient::onConnected()
{
    logToFile("========================================");
    logToFile("【重要事件】onConnected 被调用");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("连接ID: " + QString::number(m_config.id));
    logToFile("连接名称: " + m_config.name);
    logToFile("主机: " + m_config.host);
    logToFile("端口: " + QString::number(m_config.port));
    logToFile("客户端ID: " + m_config.clientId);
    logToFile("当前时间戳: " + QString::number(QDateTime::currentMSecsSinceEpoch()));
    logToFile("QMqttClient状态: " + QString::number(m_client->state()));

    m_connected = true;

    logToFile("m_connected 已设置为: true");
    logToFile("连接成功，等待上层代码执行订阅和命令恢复...");

    emit connected();

    logToFile("connected() 信号已发射");
    logToFile("onConnected 执行完毕");
    logToFile("========================================");
}

void MqttClient::onDisconnected()
{
    logToFile("========================================");
    logToFile("【重要事件】onDisconnected 被调用");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("连接ID: " + QString::number(m_config.id));
    logToFile("连接名称: " + m_config.name);

    m_connected = false;

    logToFile("m_connected 已设置为: false");
    logToFile("QMqttClient状态: " + QString::number(m_client->state()));

    emit disconnected();

    logToFile("disconnected() 信号已发射");
    logToFile("onDisconnected 执行完毕");
    logToFile("========================================");
}

void MqttClient::onMessageReceived(const QByteArray &payload, const QMqttTopicName &topic)
{
    Logger::debug("MQTT", QString("【收到消息】主题=%1 大小=%2B")
                              .arg(topic.name()).arg(payload.size()));

    QString text;
    bool isHex = false;

    // 先检测是否包含大量控制字符（即使 UTF-8 解码成功也可能是乱码）
    if (looksLikeBinary(payload)) {
        isHex = true;
        Logger::debug("MQTT", "载荷检测为二进制数据，转换为 HEX 显示");
    } else {
        auto decoder = QStringDecoder(QStringConverter::Utf8);
        text = decoder(payload);
        if (decoder.hasError()) {
            isHex = true;
            Logger::debug("MQTT", "UTF-8 解码失败，转换为 HEX 显示");
        }
    }

    if (isHex) {
        text = "HEX: " + QString::fromLatin1(payload.toHex(' ')).toUpper();
        Logger::debug("MQTT", "内容(HEX): " + text.left(120));
    } else {
        Logger::debug("MQTT", "内容(文本): " + text.left(120));
    }

    emit messageReceived(topic.name(), text, false);
}

void MqttClient::onMessageReceived(const QMqttMessage &message)
{
    const QByteArray &payload = message.payload();

    Logger::debug("MQTT", QString("【收到消息】主题=%1 QoS=%2 Retain=%3 大小=%4B")
                              .arg(message.topic().name())
                              .arg(message.qos())
                              .arg(message.retain() ? "是" : "否")
                              .arg(payload.size()));

    QString text;
    bool isHex = false;

    // 先检测是否包含大量控制字符（即使 UTF-8 解码成功也可能是乱码）
    if (looksLikeBinary(payload)) {
        isHex = true;
        Logger::debug("MQTT", "载荷检测为二进制数据，转换为 HEX 显示");
    } else {
        auto decoder = QStringDecoder(QStringConverter::Utf8);
        text = decoder(payload);
        if (decoder.hasError()) {
            isHex = true;
            Logger::debug("MQTT", "UTF-8 解码失败，转换为 HEX 显示");
        }
    }

    if (isHex) {
        text = "HEX: " + QString::fromLatin1(payload.toHex(' ')).toUpper();
        Logger::debug("MQTT", "内容(HEX): " + text.left(120));
    } else {
        Logger::debug("MQTT", "内容(文本): " + text.left(120));
    }

    emit messageReceived(message.topic().name(), text, message.retain());
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorMsg = mqttErrorString(error);

    logToFile("========================================");
    logToFile("【错误事件】onErrorChanged");
    logToFile("时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    logToFile("错误码: " + QString::number(error));
    logToFile("错误信息: " + errorMsg);
    logToFile("当前QMqttClient状态: " + QString::number(m_client->state()));
    logToFile("当前m_connected标志: " + QString(m_connected ? "true" : "false"));
    logToFile("主机: " + m_client->hostname());
    logToFile("端口: " + QString::number(m_client->port()));
    logToFile("客户端ID: " + m_client->clientId());
    logToFile("========================================");

    if (error != QMqttClient::NoError) {
        emit errorOccurred(errorMsg);
    }
}

QString MqttClient::mqttErrorString(QMqttClient::ClientError error) const
{
    switch (error) {
    case QMqttClient::NoError:
        return "No error";
    case QMqttClient::InvalidProtocolVersion:
        return "Invalid protocol version";
    case QMqttClient::IdRejected:
        return "Client ID rejected";
    case QMqttClient::ServerUnavailable:
        return "Server unavailable";
    case QMqttClient::BadUsernameOrPassword:
        return "Bad username or password";
    case QMqttClient::NotAuthorized:
        return "Not authorized";
    case QMqttClient::TransportInvalid:
        return "Transport invalid";
    case QMqttClient::ProtocolViolation:
        return "Protocol violation";
    case QMqttClient::UnknownError:
        return "Unknown error";
    default:
        return "Error " + QString::number(error);
    }
}

