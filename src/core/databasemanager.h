#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QMutexLocker>
#include <QSqlError>
#include <QList>
#include <QFile>
#include "models.h"
#include "logger.h"

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
    bool databaseFileExists() const
    {
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
    // Returns the count of received (outgoing=0) messages for a connection.
    // Uses a covered index — O(log n) even with millions of rows.
    qint64 countReceivedMessages(int connectionId);

    // ─── Schema versioning & migrations ─────────────────────────
    /** 当前数据库 schema 版本（0=旧版/未初始化）*/
    int schemaVersion() const;
    /** 最新版本号——新增迁移时请同步递增此值 */
    static int latestSchemaVersion() { return kLatestSchemaVersion; }
    /** 自动应用所有待执行的迁移，返回是否全部成功 */
    bool applyMigrations();

private:
    static const int kLatestSchemaVersion = 2;

    QSqlDatabase m_db;
    bool createTables();
    bool setSchemaVersion(int version);
};

#endif // DATABASEMANAGER_H
