#include "logger.h"
#include <QCoreApplication>
#include <QMutexLocker>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────
//  Singleton
// ─────────────────────────────────────────────────────────────

Logger &Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent) : QObject(parent)
{
    // 默认日志目录：应用程序工作目录
    m_logDir = QDir::currentPath();
    openLogFile();
}

Logger::~Logger()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        m_file.close();
}

// ─────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────

void Logger::setMinLevel(Level level)
{
    QMutexLocker lock(&m_mutex);
    m_minLevel = level;
}

void Logger::setLogDir(const QString &dirPath)
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        m_file.close();
    m_logDir = dirPath;
    QDir().mkpath(dirPath);
    openLogFile();
}

QString Logger::logFilePath() const
{
    return m_file.fileName();
}

// ─────────────────────────────────────────────────────────────
//  Core log method
// ─────────────────────────────────────────────────────────────

void Logger::log(Level level, const QString &category, const QString &message)
{
    QMutexLocker lock(&m_mutex);

    if (level < m_minLevel)
        return;

    static const char *kLevelStr[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
    QDateTime now = QDateTime::currentDateTime();
    QString line = QString("[%1] [%2] [%3] %4")
                       .arg(now.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                       .arg(kLevelStr[static_cast<int>(level)])
                       .arg(category, -12)
                       .arg(message);

    writeToFile(line);

    // Emit signal (queued if called from non-GUI thread)
    emit logAppended(static_cast<int>(level), category, message, now);
}

void Logger::separator(const QString &title)
{
    QMutexLocker lock(&m_mutex);
    QString line;
    if (title.isEmpty()) {
        line = QString(60, '=');
    } else {
        int padLen = qMax(0, (60 - title.length() - 4) / 2);
        QString pad(padLen, '=');
        line = pad + "  " + title + "  " + pad;
    }
    writeToFile(line);
}

// ─────────────────────────────────────────────────────────────
//  Static convenience methods
// ─────────────────────────────────────────────────────────────

void Logger::debug(const QString &category, const QString &message)
{
    instance().log(DEBUG, category, message);
}

void Logger::info(const QString &category, const QString &message)
{
    instance().log(INFO, category, message);
}

void Logger::warning(const QString &category, const QString &message)
{
    instance().log(WARNING, category, message);
}

void Logger::error(const QString &category, const QString &message)
{
    instance().log(ERROR, category, message);
}

// ─────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────

void Logger::openLogFile()
{
    // 日志文件：mqtt_assistant_YYYY-MM-DD.log，放在 m_logDir 下
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString path    = m_logDir + "/mqtt_assistant_" + dateStr + ".log";

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::Append | QIODevice::Text)) {
        // 退回到临时目录
        QString tmpPath = QDir::tempPath() + "/mqtt_assistant_" + dateStr + ".log";
        m_file.setFileName(tmpPath);
        m_file.open(QIODevice::Append | QIODevice::Text);
    }

    // 写入启动分隔符（不加锁，调用者已经加锁或在构造函数中）
    if (m_file.isOpen()) {
        QTextStream out(&m_file);
        out << "\n";
        out << QString(60, '=') << "\n";
        out << QString("[%1] ====  MQTT Assistant 启动  ====\n")
                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        out << QString(60, '=') << "\n";
        m_file.flush();
    }
}

void Logger::writeToFile(const QString &line)
{
    // 注意：调用方已持有 m_mutex
    if (!m_file.isOpen())
        return;

    QTextStream out(&m_file);
    out << line << "\n";
    m_file.flush();
}
