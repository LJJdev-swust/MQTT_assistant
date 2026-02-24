#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "models.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool open();
    void close();

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

    // Messages
    int saveMessage(const MessageRecord &msg);
    QList<MessageRecord> loadMessages(int connectionId, int limit = 100);
    bool deleteMessages(int connectionId);

private:
    QSqlDatabase m_db;
    bool createTables();
};

#endif // DATABASEMANAGER_H
