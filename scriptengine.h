#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QTimer>
#include "models.h"

class MqttClient;

class ScriptEngine : public QObject
{
    Q_OBJECT
public:
    explicit ScriptEngine(QObject *parent = nullptr);
    ~ScriptEngine();

    void setClient(MqttClient *client);
    void setScripts(const QList<ScriptConfig> &scripts);
    void addScript(const ScriptConfig &script);
    void updateScript(const ScriptConfig &script);
    void removeScript(int scriptId);
    void clearScripts();
    QList<ScriptConfig> scripts() const { return m_scripts; }

signals:
    void messagePublished(const QString &topic, const QString &payload);

public slots:
    void onMessageReceived(const QString &topic, const QString &payload, bool retained);

private:
    bool matchesCondition(const ScriptConfig &script, const QString &topic, const QString &payload) const;
    QString substituteVariables(const QString &tmpl, const QString &topic, const QString &payload) const;
    void triggerScript(const ScriptConfig &script, const QString &topic, const QString &payload);

    MqttClient *m_client;
    QList<ScriptConfig> m_scripts;
};

#endif // SCRIPTENGINE_H
