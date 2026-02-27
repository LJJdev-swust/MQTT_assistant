#ifndef COMMANDPANEL_H
#define COMMANDPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include <QTimer>
#include "core/models.h"

class MqttClient;

class CommandPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CommandPanel(QWidget *parent = nullptr);
    ~CommandPanel();

    void setClient(MqttClient *client);
    void addCommand(const CommandConfig &cmd);
    void updateCommand(const CommandConfig &cmd);
    void removeCommand(int id);
    void clearCommands();

signals:
    void editRequested(int commandId);
    void deleteRequested(int commandId);
    void addRequested();
    void commandSent(const QString &topic, const QString &payload);

private slots:
    void onContextMenu(const QPoint &pos);
    void onLoopTimer();

private:
    void sendCommand(int commandId);
    void startLoop(int commandId);
    void stopLoop(int commandId);
    QListWidgetItem *findItem(int commandId) const;

    QListWidget *m_listWidget;
    MqttClient  *m_client;
    QMap<int, CommandConfig> m_commands;   // id -> config
    QMap<int, QTimer*>       m_loopTimers; // id -> timer
};

#endif // COMMANDPANEL_H
