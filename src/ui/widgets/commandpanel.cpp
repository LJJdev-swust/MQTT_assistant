#include "commandpanel.h"
#include "core/mqttclient.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

CommandPanel::CommandPanel(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &CommandPanel::onContextMenu);
}

CommandPanel::~CommandPanel()
{
    qDeleteAll(m_loopTimers);
}

void CommandPanel::setClient(MqttClient *client)
{
    m_client = client;
}

void CommandPanel::addCommand(const CommandConfig &cmd)
{
    m_commands[cmd.id] = cmd;
    QListWidgetItem *item = new QListWidgetItem(cmd.name, m_listWidget);
    item->setData(Qt::UserRole, cmd.id);
    m_listWidget->addItem(item);
}

void CommandPanel::updateCommand(const CommandConfig &cmd)
{
    m_commands[cmd.id] = cmd;
    QListWidgetItem *item = findItem(cmd.id);
    if (item)
        item->setText(cmd.name);
}

void CommandPanel::removeCommand(int id)
{
    stopLoop(id);
    m_commands.remove(id);
    QListWidgetItem *item = findItem(id);
    if (item)
        delete m_listWidget->takeItem(m_listWidget->row(item));
}

void CommandPanel::clearCommands()
{
    for (int id : m_loopTimers.keys())
        stopLoop(id);
    m_commands.clear();
    m_listWidget->clear();
}

void CommandPanel::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    int id = item->data(Qt::UserRole).toInt();
    bool looping = m_loopTimers.contains(id);

    QMenu menu(this);
    QAction *actSend      = menu.addAction("Send");
    QAction *actStartLoop = menu.addAction("Start Loop");
    QAction *actStopLoop  = menu.addAction("Stop Loop");
    menu.addSeparator();
    QAction *actEdit   = menu.addAction("Edit");
    QAction *actDelete = menu.addAction("Delete");

    actStartLoop->setEnabled(!looping);
    actStopLoop->setEnabled(looping);

    QAction *chosen = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actSend)      sendCommand(id);
    if (chosen == actStartLoop) startLoop(id);
    if (chosen == actStopLoop)  stopLoop(id);
    if (chosen == actEdit)      emit editRequested(id);
    if (chosen == actDelete)    emit deleteRequested(id);
}

void CommandPanel::onLoopTimer()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    int id = m_loopTimers.key(timer, -1);
    if (id != -1)
        sendCommand(id);
}

void CommandPanel::sendCommand(int commandId)
{
    if (!m_client || !m_client->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect to an MQTT broker first.");
        return;
    }
    if (!m_commands.contains(commandId)) return;
    const CommandConfig &cmd = m_commands[commandId];
    m_client->publish(cmd.topic, cmd.payload, cmd.qos, cmd.retain);
}

void CommandPanel::startLoop(int commandId)
{
    if (m_loopTimers.contains(commandId)) return;
    if (!m_commands.contains(commandId)) return;
    const CommandConfig &cmd = m_commands[commandId];
    int interval = qMax(100, cmd.loopIntervalMs);

    QTimer *timer = new QTimer(this);
    timer->setInterval(interval);
    connect(timer, &QTimer::timeout, this, &CommandPanel::onLoopTimer);
    m_loopTimers[commandId] = timer;
    timer->start();

    // Update item display to indicate looping
    QListWidgetItem *item = findItem(commandId);
    if (item)
        item->setText(cmd.name + " [looping]");
}

void CommandPanel::stopLoop(int commandId)
{
    if (!m_loopTimers.contains(commandId)) return;
    QTimer *timer = m_loopTimers.take(commandId);
    timer->stop();
    timer->deleteLater();

    // Restore item display
    QListWidgetItem *item = findItem(commandId);
    if (item && m_commands.contains(commandId))
        item->setText(m_commands[commandId].name);
}

QListWidgetItem *CommandPanel::findItem(int commandId) const
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == commandId)
            return item;
    }
    return nullptr;
}
