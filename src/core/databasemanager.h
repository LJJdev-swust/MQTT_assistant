#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QList>
#include <QFile>
#include "models.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool open(const QString &dbPath = QString());
    void close();

    // 新增：数据库路径和存在性检查
    QString databasePath() const { return m_db.databaseName(); }
    bool databaseFileExists() const {
        QString path = m_db.databaseName();
        return !path.isEmpty() && QFile::exists(path);
    }
    QString lastError() const { return m_db.lastError().text(); }

    // Connections
    QList<MqttConnectionConfig> loadConnections();
    int saveConnection(const MqttConnectionConfig &config);
    bool updateConnection(const MqttConnectionConfig &config);
    bool deleteConnection(int id);

    // Commands
    QList<CommandConfig> loadCommands();
    int saveCommand(const CommandConfig &cmd);
    bool updateCommand(const CommandConfig &cmd);
    bool deleteCommand(int id);

    // Scripts
    QList<ScriptConfig> loadScripts();
    int saveScript(const ScriptConfig &script);
    bool updateScript(const ScriptConfig &script);
    bool deleteScript(int id);

    // Subscriptions
    QList<SubscriptionConfig> loadSubscriptions(int connectionId);
    int saveSubscription(const SubscriptionConfig &sub);
    bool deleteSubscription(int id);

    // Messages
    int saveMessage(const MessageRecord &msg);
    QList<MessageRecord> loadMessages(int connectionId, int limit = 100);
    bool deleteMessages(int connectionId);

private:
    QSqlDatabase m_db;
    bool createTables();
};

#endif // DATABASEMANAGER_H
