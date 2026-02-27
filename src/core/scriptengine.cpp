#include "scriptengine.h"
#include "mqttclient.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>

ScriptEngine::ScriptEngine(QObject *parent)
    : QObject(parent)
    , m_client(nullptr)
{
}

ScriptEngine::~ScriptEngine() {}

void ScriptEngine::setClient(MqttClient *client)
{
    if (m_client) {
        disconnect(m_client, &MqttClient::messageReceived, this, &ScriptEngine::onMessageReceived);
    }
    m_client = client;
    if (m_client) {
        // 确保参数匹配
        connect(m_client, &MqttClient::messageReceived,
                this, &ScriptEngine::onMessageReceived);
    }
}

void ScriptEngine::setScripts(const QList<ScriptConfig> &scripts)
{
    m_scripts = scripts;
}

void ScriptEngine::addScript(const ScriptConfig &script)
{
    m_scripts.append(script);
}

void ScriptEngine::updateScript(const ScriptConfig &script)
{
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts[i].id == script.id) {
            m_scripts[i] = script;
            return;
        }
    }
    m_scripts.append(script);
}

void ScriptEngine::removeScript(int scriptId)
{
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts[i].id == scriptId) {
            m_scripts.removeAt(i);
            return;
        }
    }
}

void ScriptEngine::clearScripts()
{
    m_scripts.clear();
}

void ScriptEngine::onMessageReceived(const QString &topic, const QString &payload, bool retained)
{
    if (retained)
        return;

    if (!m_client || !m_client->isConnected())
        return;

    for (const ScriptConfig &script : m_scripts) {
        if (!script.enabled)
            continue;

        // Topic filter: empty means any topic
        if (!script.triggerTopic.isEmpty()) {
            // Simple wildcard matching: # and +
            QRegularExpression topicRx(
                "^" + QRegularExpression::escape(script.triggerTopic)
                          .replace("\\#", ".*")
                          .replace("\\+", "[^/]+") + "$");
            if (!topicRx.match(topic).hasMatch())
                continue;
        }

        if (matchesCondition(script, topic, payload)) {
            triggerScript(script, topic, payload);
        }
    }
}

bool ScriptEngine::matchesCondition(const ScriptConfig &script,
                                    const QString &topic,
                                    const QString &payload) const
{
    Q_UNUSED(topic)
    const QString &cond  = script.triggerCondition;
    const QString &value = script.triggerValue;

    if (cond == "any")
        return true;
    if (cond == "contains")
        return payload.contains(value);
    if (cond == "equals")
        return payload == value;
    if (cond == "startsWith")
        return payload.startsWith(value);
    if (cond == "endsWith")
        return payload.endsWith(value);
    if (cond == "regex") {
        QRegularExpression rx(value);
        return rx.match(payload).hasMatch();
    }
    return false;
}

QString ScriptEngine::substituteVariables(const QString &tmpl,
                                          const QString &topic,
                                          const QString &payload) const
{
    QString result = tmpl;

    // 原有的 ISO 格式时间戳（返回字符串）
    result.replace("{{timestamp}}", QDateTime::currentDateTime().toString(Qt::ISODate));

    // 添加 Unix 时间戳（秒级，返回数字）
    result.replace("{{timestamp_unix}}", QString::number(QDateTime::currentSecsSinceEpoch()));

    // 添加毫秒级时间戳
    result.replace("{{timestamp_ms}}", QString::number(QDateTime::currentMSecsSinceEpoch()));

    // 原有的变量替换
    result.replace("{{topic}}",     topic);
    result.replace("{{payload}}",   payload);

    return result;
}

void ScriptEngine::triggerScript(const ScriptConfig &script,
                                 const QString &topic,
                                 const QString &payload)
{
    QString responseTopic   = substituteVariables(script.responseTopic, topic, payload);
    QString responsePayload = substituteVariables(script.responsePayload, topic, payload);
    int qos     = script.responseQos;
    bool retain = script.responseRetain;

    // 添加调试输出，查看触发次数
    qDebug() << "脚本触发 - ID:" << script.id
             << "名称:" << script.name
             << "响应主题:" << responseTopic
             << "响应内容:" << responsePayload;

    if (script.delayMs <= 0) {
        QMetaObject::invokeMethod(m_client, "publish", Qt::QueuedConnection,
                                  Q_ARG(QString, responseTopic),
                                  Q_ARG(QString, responsePayload),
                                  Q_ARG(int, qos),
                                  Q_ARG(bool, retain));
        qDebug() << "立即发布消息";
        emit messagePublished(responseTopic, responsePayload);
    } else {
        qDebug() << "延迟" << script.delayMs << "ms后发布消息";
        // 使用一个标志位防止多次触发（如果需要）
        QTimer::singleShot(script.delayMs, this, [this, responseTopic, responsePayload, qos, retain]() {
            if (m_client && m_client->isConnected()) {
                QMetaObject::invokeMethod(m_client, "publish", Qt::QueuedConnection,
                                          Q_ARG(QString, responseTopic),
                                          Q_ARG(QString, responsePayload),
                                          Q_ARG(int, qos),
                                          Q_ARG(bool, retain));
                qDebug() << "延迟后发布消息";
                emit messagePublished(responseTopic, responsePayload);
            }
        });
    }
}
