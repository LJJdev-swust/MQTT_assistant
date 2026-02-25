#include "connectionpanel.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPainter>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &ConnectionPanel::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &ConnectionPanel::onContextMenu);
    connect(m_listWidget, &QListWidget::currentItemChanged,
            [this](QListWidgetItem *cur, QListWidgetItem *) {
                if (cur)
                    emit selectionChanged(cur->data(Qt::UserRole).toInt());
            });
}

void ConnectionPanel::addConnection(const MqttConnectionConfig &config, bool connected)
{
    m_connectedState[config.id] = connected;
    QListWidgetItem *item = new QListWidgetItem(m_listWidget);
    item->setData(Qt::UserRole,     config.id);
    item->setData(Qt::UserRole + 1, config.name); // store name separately
    updateItemDisplay(item, config.id, config.name);
    m_listWidget->addItem(item);
}

void ConnectionPanel::updateConnection(const MqttConnectionConfig &config)
{
    QListWidgetItem *item = findItem(config.id);
    if (item) {
        item->setData(Qt::UserRole + 1, config.name);
        updateItemDisplay(item, config.id, config.name);
    }
}

void ConnectionPanel::removeConnection(int id)
{
    QListWidgetItem *item = findItem(id);
    if (item) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
        m_connectedState.remove(id);
    }
}

void ConnectionPanel::setConnected(int id, bool connected)
{
    m_connectedState[id] = connected;
    QListWidgetItem *item = findItem(id);
    if (item) {
        QString name = item->data(Qt::UserRole + 1).toString();
        updateItemDisplay(item, id, name);
    }
}

void ConnectionPanel::clearConnections()
{
    m_listWidget->clear();
    m_connectedState.clear();
}

int ConnectionPanel::selectedConnectionId() const
{
    QListWidgetItem *item = m_listWidget->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void ConnectionPanel::updateItemDisplay(QListWidgetItem *item, int connectionId, const QString &name)
{
    bool connected = m_connectedState.value(connectionId, false);
    // Use unicode circle as status dot
    QString dot = connected ? QString::fromUtf8("\u25CF") : QString::fromUtf8("\u25CB");
    item->setText(dot + " " + name);
    item->setForeground(connected ? QColor("#4caf50") : QColor("#a0a0b0"));
    item->setToolTip(connected ? "已连接" : "未连接");
}

QListWidgetItem *ConnectionPanel::findItem(int connectionId) const
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == connectionId)
            return item;
    }
    return nullptr;
}

void ConnectionPanel::onItemDoubleClicked(QListWidgetItem *item)
{
    int id = item->data(Qt::UserRole).toInt();
    if (m_connectedState.value(id, false))
        emit disconnectRequested(id);
    else
        emit connectRequested(id);
}

void ConnectionPanel::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    int id = item->data(Qt::UserRole).toInt();
    bool connected = m_connectedState.value(id, false);

    QMenu menu(this);
    QAction *actConnect    = menu.addAction("连接");
    QAction *actDisconnect = menu.addAction("断开连接");
    menu.addSeparator();
    QAction *actEdit   = menu.addAction("编辑");
    QAction *actDelete = menu.addAction("删除");

    actConnect->setEnabled(!connected);
    actDisconnect->setEnabled(connected);

    QAction *chosen = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actConnect)    emit connectRequested(id);
    if (chosen == actDisconnect) emit disconnectRequested(id);
    if (chosen == actEdit)       emit editRequested(id);
    if (chosen == actDelete)     emit deleteRequested(id);
}
