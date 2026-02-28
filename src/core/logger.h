#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

/**
 * @brief 线程安全的单例日志系统
 *
 * 使用方法：
 *   Logger::debug("MQTT",   "连接到主机: xxx");
 *   Logger::info("DB",      "数据库迁移完成, 版本: 2");
 *   Logger::warning("Script","脚本触发但连接未激活");
 *   Logger::error("MQTT",   "连接失败: xxx");
 */
class Logger : public QObject
{
    Q_OBJECT
public:
    enum Level {
        DEBUG   = 0,
        INFO    = 1,
        WARNING = 2,
        ERROR   = 3
    };
    Q_ENUM(Level)

    static Logger &instance();

    // 便捷静态接口
    static void debug  (const QString &category, const QString &message);
    static void info   (const QString &category, const QString &message);
    static void warning(const QString &category, const QString &message);
    static void error  (const QString &category, const QString &message);

    // 通用接口
    void log(Level level, const QString &category, const QString &message);

    // 配置
    void setMinLevel(Level level);
    void setLogDir(const QString &dirPath);
    QString logFilePath() const;

    // 写入一段分隔线（用于标记启动/关键阶段）
    void separator(const QString &title = QString());

signals:
    /** 每条日志会通过此信号发射，供UI显示 */
    void logAppended(int level, const QString &category,
                     const QString &message, const QDateTime &time);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();

    void openLogFile();
    void writeToFile(const QString &line);

    QMutex   m_mutex;
    QFile    m_file;
    Level    m_minLevel = DEBUG;
    QString  m_logDir;
};

#endif // LOGGER_H
