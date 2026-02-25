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
        connect(m_client, &MqttClient::messageReceived, this, &ScriptEngine::onMessageReceived);
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

void ScriptEngine::onMessageReceived(const QString &topic, const QString &payload)
{
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
    result.replace("{{timestamp}}", QDateTime::currentDateTime().toString(Qt::ISODate));
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

    if (script.delayMs <= 0) {
        m_client->publish(responseTopic, responsePayload, qos, retain);
    } else {
        // Capture by value for deferred execution
        QTimer::singleShot(script.delayMs, this, [this, responseTopic, responsePayload, qos, retain]() {
            if (m_client && m_client->isConnected())
                m_client->publish(responseTopic, responsePayload, qos, retain);
        });
    }
}
