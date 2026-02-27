#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QVariant>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString &dbPath)
{
    QString path = dbPath;
    if (path.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        path = dataDir + "/mqtt_assistant.db";
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
    }

    // 关键修复：如果已经存在同名连接，先关闭并移除它
    QString connectionName = "mqtt_assistant_db";
    if (QSqlDatabase::contains(connectionName)) {
        // Close and invalidate m_db before removing so Qt doesn't warn
        // about a connection still in use.
        if (m_db.isOpen())
            m_db.close();
        m_db = QSqlDatabase();          // release the reference
        QSqlDatabase::removeDatabase(connectionName);
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "数据库成功打开，路径：" << path;
    return createTables();
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::createTables()
{
    QSqlQuery q(m_db);

    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS connections ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "host TEXT NOT NULL DEFAULT 'localhost',"
        "port INTEGER NOT NULL DEFAULT 1883,"
        "username TEXT,"
        "password TEXT,"
        "client_id TEXT,"
        "use_tls INTEGER NOT NULL DEFAULT 0,"
        "ca_cert_path TEXT,"
        "client_cert_path TEXT,"
        "client_key_path TEXT,"
        "clean_session INTEGER NOT NULL DEFAULT 1,"
        "keep_alive INTEGER NOT NULL DEFAULT 60"
        ")"
        );
    if (!ok) { qWarning() << q.lastError().text(); return false; }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS commands ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "topic TEXT NOT NULL,"
        "payload TEXT,"
        "qos INTEGER NOT NULL DEFAULT 0,"
        "retain INTEGER NOT NULL DEFAULT 0,"
        "loop_enabled INTEGER NOT NULL DEFAULT 0,"
        "loop_interval_ms INTEGER NOT NULL DEFAULT 1000,"
        "connection_id INTEGER NOT NULL DEFAULT -1"
        ")"
        );
    if (!ok) { qWarning() << q.lastError().text(); return false; }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS scripts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "enabled INTEGER NOT NULL DEFAULT 1,"
        "trigger_topic TEXT,"
        "trigger_condition TEXT NOT NULL DEFAULT 'any',"
        "trigger_value TEXT,"
        "response_topic TEXT,"
        "response_payload TEXT,"
        "response_qos INTEGER NOT NULL DEFAULT 0,"
        "response_retain INTEGER NOT NULL DEFAULT 0,"
        "delay_ms INTEGER NOT NULL DEFAULT 0,"
        "connection_id INTEGER NOT NULL DEFAULT -1"
        ")"
        );
    if (!ok) { qWarning() << q.lastError().text(); return false; }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS subscriptions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id INTEGER NOT NULL,"
        "topic TEXT NOT NULL,"
        "qos INTEGER NOT NULL DEFAULT 0"
        ")"
        );
    if (!ok) { qWarning() << q.lastError().text(); return false; }

    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "connection_id INTEGER NOT NULL,"
        "topic TEXT NOT NULL,"
        "payload TEXT,"
        "outgoing INTEGER NOT NULL DEFAULT 0,"
        "timestamp TEXT NOT NULL"
        ")"
        );
    if (!ok) { qWarning() << q.lastError().text(); return false; }

    return true;
}

// ---- Connections ----

QList<MqttConnectionConfig> DatabaseManager::loadConnections()
{
    QList<MqttConnectionConfig> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id,name,host,port,username,password,client_id,"
           "use_tls,ca_cert_path,client_cert_path,client_key_path,"
           "clean_session,keep_alive FROM connections ORDER BY id");
    while (q.next()) {
        MqttConnectionConfig c;
        c.id              = q.value(0).toInt();
        c.name            = q.value(1).toString();
        c.host            = q.value(2).toString();
        c.port            = q.value(3).toInt();
        c.username        = q.value(4).toString();
        c.password        = q.value(5).toString();
        c.clientId        = q.value(6).toString();
        c.useTLS          = q.value(7).toBool();
        c.caCertPath      = q.value(8).toString();
        c.clientCertPath  = q.value(9).toString();
        c.clientKeyPath   = q.value(10).toString();
        c.cleanSession    = q.value(11).toBool();
        c.keepAlive       = q.value(12).toInt();
        list.append(c);
    }
    return list;
}

int DatabaseManager::saveConnection(const MqttConnectionConfig &config)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO connections (name,host,port,username,password,client_id,"
              "use_tls,ca_cert_path,client_cert_path,client_key_path,clean_session,keep_alive) "
              "VALUES (:name,:host,:port,:user,:pass,:cid,:tls,:ca,:cc,:ck,:cs,:ka)");
    q.bindValue(":name", config.name);
    q.bindValue(":host", config.host);
    q.bindValue(":port", config.port);
    q.bindValue(":user", config.username);
    q.bindValue(":pass", config.password);
    q.bindValue(":cid",  config.clientId);
    q.bindValue(":tls",  config.useTLS ? 1 : 0);
    q.bindValue(":ca",   config.caCertPath);
    q.bindValue(":cc",   config.clientCertPath);
    q.bindValue(":ck",   config.clientKeyPath);
    q.bindValue(":cs",   config.cleanSession ? 1 : 0);
    q.bindValue(":ka",   config.keepAlive);
    if (!q.exec()) { qWarning() << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::updateConnection(const MqttConnectionConfig &config)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE connections SET name=:name,host=:host,port=:port,username=:user,"
              "password=:pass,client_id=:cid,use_tls=:tls,ca_cert_path=:ca,"
              "client_cert_path=:cc,client_key_path=:ck,clean_session=:cs,keep_alive=:ka "
              "WHERE id=:id");
    q.bindValue(":name", config.name);
    q.bindValue(":host", config.host);
    q.bindValue(":port", config.port);
    q.bindValue(":user", config.username);
    q.bindValue(":pass", config.password);
    q.bindValue(":cid",  config.clientId);
    q.bindValue(":tls",  config.useTLS ? 1 : 0);
    q.bindValue(":ca",   config.caCertPath);
    q.bindValue(":cc",   config.clientCertPath);
    q.bindValue(":ck",   config.clientKeyPath);
    q.bindValue(":cs",   config.cleanSession ? 1 : 0);
    q.bindValue(":ka",   config.keepAlive);
    q.bindValue(":id",   config.id);
    if (!q.exec()) { qWarning() << q.lastError().text(); return false; }
    return true;
}

bool DatabaseManager::deleteConnection(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM connections WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ---- Commands ----

QList<CommandConfig> DatabaseManager::loadCommands()
{
    QList<CommandConfig> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id,name,topic,payload,qos,retain,loop_enabled,loop_interval_ms,connection_id "
           "FROM commands ORDER BY id");
    while (q.next()) {
        CommandConfig c;
        c.id             = q.value(0).toInt();
        c.name           = q.value(1).toString();
        c.topic          = q.value(2).toString();
        c.payload        = q.value(3).toString();
        c.qos            = q.value(4).toInt();
        c.retain         = q.value(5).toBool();
        c.loopEnabled    = q.value(6).toBool();
        c.loopIntervalMs = q.value(7).toInt();
        c.connectionId   = q.value(8).toInt();
        list.append(c);
    }
    return list;
}

int DatabaseManager::saveCommand(const CommandConfig &cmd)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO commands (name,topic,payload,qos,retain,loop_enabled,loop_interval_ms,connection_id) "
              "VALUES (:name,:topic,:payload,:qos,:retain,:loop,:interval,:connid)");
    q.bindValue(":name",     cmd.name);
    q.bindValue(":topic",    cmd.topic);
    q.bindValue(":payload",  cmd.payload);
    q.bindValue(":qos",      cmd.qos);
    q.bindValue(":retain",   cmd.retain ? 1 : 0);
    q.bindValue(":loop",     cmd.loopEnabled ? 1 : 0);
    q.bindValue(":interval", cmd.loopIntervalMs);
    q.bindValue(":connid",   cmd.connectionId);
    if (!q.exec()) { qWarning() << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::updateCommand(const CommandConfig &cmd)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE commands SET name=:name,topic=:topic,payload=:payload,qos=:qos,"
              "retain=:retain,loop_enabled=:loop,loop_interval_ms=:interval,connection_id=:connid "
              "WHERE id=:id");
    q.bindValue(":name",     cmd.name);
    q.bindValue(":topic",    cmd.topic);
    q.bindValue(":payload",  cmd.payload);
    q.bindValue(":qos",      cmd.qos);
    q.bindValue(":retain",   cmd.retain ? 1 : 0);
    q.bindValue(":loop",     cmd.loopEnabled ? 1 : 0);
    q.bindValue(":interval", cmd.loopIntervalMs);
    q.bindValue(":connid",   cmd.connectionId);
    q.bindValue(":id",       cmd.id);
    if (!q.exec()) { qWarning() << q.lastError().text(); return false; }
    return true;
}

bool DatabaseManager::deleteCommand(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM commands WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ---- Scripts ----

QList<ScriptConfig> DatabaseManager::loadScripts()
{
    QList<ScriptConfig> list;
    QSqlQuery q(m_db);
    q.exec("SELECT id,name,enabled,trigger_topic,trigger_condition,trigger_value,"
           "response_topic,response_payload,response_qos,response_retain,delay_ms,connection_id "
           "FROM scripts ORDER BY id");
    while (q.next()) {
        ScriptConfig s;
        s.id               = q.value(0).toInt();
        s.name             = q.value(1).toString();
        s.enabled          = q.value(2).toBool();
        s.triggerTopic     = q.value(3).toString();
        s.triggerCondition = q.value(4).toString();
        s.triggerValue     = q.value(5).toString();
        s.responseTopic    = q.value(6).toString();
        s.responsePayload  = q.value(7).toString();
        s.responseQos      = q.value(8).toInt();
        s.responseRetain   = q.value(9).toBool();
        s.delayMs          = q.value(10).toInt();
        s.connectionId     = q.value(11).toInt();
        list.append(s);
    }
    return list;
}

int DatabaseManager::saveScript(const ScriptConfig &script)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO scripts (name,enabled,trigger_topic,trigger_condition,trigger_value,"
              "response_topic,response_payload,response_qos,response_retain,delay_ms,connection_id) "
              "VALUES (:name,:enabled,:ttopic,:tcond,:tval,:rtopic,:rpayload,:rqos,:rretain,:delay,:connid)");
    q.bindValue(":name",     script.name);
    q.bindValue(":enabled",  script.enabled ? 1 : 0);
    q.bindValue(":ttopic",   script.triggerTopic);
    q.bindValue(":tcond",    script.triggerCondition);
    q.bindValue(":tval",     script.triggerValue);
    q.bindValue(":rtopic",   script.responseTopic);
    q.bindValue(":rpayload", script.responsePayload);
    q.bindValue(":rqos",     script.responseQos);
    q.bindValue(":rretain",  script.responseRetain ? 1 : 0);
    q.bindValue(":delay",    script.delayMs);
    q.bindValue(":connid",   script.connectionId);
    if (!q.exec()) { qWarning() << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::updateScript(const ScriptConfig &script)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE scripts SET name=:name,enabled=:enabled,trigger_topic=:ttopic,"
              "trigger_condition=:tcond,trigger_value=:tval,response_topic=:rtopic,"
              "response_payload=:rpayload,response_qos=:rqos,response_retain=:rretain,"
              "delay_ms=:delay,connection_id=:connid WHERE id=:id");
    q.bindValue(":name",     script.name);
    q.bindValue(":enabled",  script.enabled ? 1 : 0);
    q.bindValue(":ttopic",   script.triggerTopic);
    q.bindValue(":tcond",    script.triggerCondition);
    q.bindValue(":tval",     script.triggerValue);
    q.bindValue(":rtopic",   script.responseTopic);
    q.bindValue(":rpayload", script.responsePayload);
    q.bindValue(":rqos",     script.responseQos);
    q.bindValue(":rretain",  script.responseRetain ? 1 : 0);
    q.bindValue(":delay",    script.delayMs);
    q.bindValue(":connid",   script.connectionId);
    q.bindValue(":id",       script.id);
    if (!q.exec()) { qWarning() << q.lastError().text(); return false; }
    return true;
}

bool DatabaseManager::deleteScript(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM scripts WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ---- Subscriptions ----

QList<SubscriptionConfig> DatabaseManager::loadSubscriptions(int connectionId)
{
    QList<SubscriptionConfig> list;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,connection_id,topic,qos FROM subscriptions WHERE connection_id=:connid ORDER BY id");
    q.bindValue(":connid", connectionId);
    if (!q.exec()) { qWarning() << q.lastError().text(); return list; }
    while (q.next()) {
        SubscriptionConfig s;
        s.id           = q.value(0).toInt();
        s.connectionId = q.value(1).toInt();
        s.topic        = q.value(2).toString();
        s.qos          = q.value(3).toInt();
        list.append(s);
    }
    return list;
}

int DatabaseManager::saveSubscription(const SubscriptionConfig &sub)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO subscriptions (connection_id,topic,qos) VALUES (:connid,:topic,:qos)");
    q.bindValue(":connid", sub.connectionId);
    q.bindValue(":topic",  sub.topic);
    q.bindValue(":qos",    sub.qos);
    if (!q.exec()) { qWarning() << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool DatabaseManager::deleteSubscription(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM subscriptions WHERE id=:id");
    q.bindValue(":id", id);
    return q.exec();
}

// ---- Messages ----

int DatabaseManager::saveMessage(const MessageRecord &msg)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO messages (connection_id,topic,payload,outgoing,timestamp) "
              "VALUES (:connid,:topic,:payload,:out,:ts)");
    q.bindValue(":connid",  msg.connectionId);
    q.bindValue(":topic",   msg.topic);
    q.bindValue(":payload", msg.payload);
    q.bindValue(":out",     msg.outgoing ? 1 : 0);
    q.bindValue(":ts",      msg.timestamp.toString(Qt::ISODate));
    if (!q.exec()) { qWarning() << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

QList<MessageRecord> DatabaseManager::loadMessages(int connectionId, int limit)
{
    QList<MessageRecord> list;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,connection_id,topic,payload,outgoing,timestamp FROM messages "
              "WHERE connection_id=:connid ORDER BY id DESC LIMIT :lim");
    q.bindValue(":connid", connectionId);
    q.bindValue(":lim",    limit);
    if (!q.exec()) { qWarning() << q.lastError().text(); return list; }

    while (q.next()) {
        MessageRecord m;
        m.id           = q.value(0).toInt();
        m.connectionId = q.value(1).toInt();
        m.topic        = q.value(2).toString();
        m.payload      = q.value(3).toString();
        m.outgoing     = q.value(4).toBool();
        m.timestamp    = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        list.prepend(m);
    }
    return list;
}

bool DatabaseManager::deleteMessages(int connectionId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM messages WHERE connection_id=:connid");
    q.bindValue(":connid", connectionId);
    return q.exec();
}
