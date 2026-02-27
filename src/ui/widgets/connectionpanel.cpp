#include "connectionpanel.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPainter>

const char *ConnectionPanel::kSpinnerFrames[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
const int ConnectionPanel::kSpinnerFrameCount = 10;

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
    , m_spinnerTimer(new QTimer(this))
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

    m_spinnerTimer->setInterval(100);
    connect(m_spinnerTimer, &QTimer::timeout, this, &ConnectionPanel::onSpinnerTick);
}

ConnectionPanel::~ConnectionPanel()
{
}

void ConnectionPanel::addConnection(const MqttConnectionConfig &config, bool connected)
{
    m_connectedState[config.id] = connected;
    m_loadingState[config.id]   = false;
    m_spinnerFrame[config.id]   = 0;
    m_unreadCount[config.id]    = 0;
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
        m_loadingState.remove(id);
        m_spinnerFrame.remove(id);
        m_unreadCount.remove(id);
        if (!hasAnyLoading())
            m_spinnerTimer->stop();
    }
}

void ConnectionPanel::setConnected(int id, bool connected)
{
    m_connectedState[id] = connected;
    m_loadingState[id]   = false;
    if (!hasAnyLoading())
        m_spinnerTimer->stop();

    QListWidgetItem *item = findItem(id);
    if (item) {
        QString name = item->data(Qt::UserRole + 1).toString();
        updateItemDisplay(item, id, name);
    }
}

void ConnectionPanel::setLoading(int id, bool loading)
{
    m_loadingState[id] = loading;
    m_spinnerFrame[id] = 0;
    if (loading) {
        if (!m_spinnerTimer->isActive())
            m_spinnerTimer->start();
    } else {
        if (!hasAnyLoading())
            m_spinnerTimer->stop();
        QListWidgetItem *item = findItem(id);
        if (item) {
            QString name = item->data(Qt::UserRole + 1).toString();
            updateItemDisplay(item, id, name);
        }
    }
}

void ConnectionPanel::clearConnections()
{
    m_listWidget->clear();
    m_connectedState.clear();
    m_loadingState.clear();
    m_spinnerFrame.clear();
    m_unreadCount.clear();
    m_spinnerTimer->stop();
}

void ConnectionPanel::setUnreadCount(int id, int count)
{
    m_unreadCount[id] = count;
    QListWidgetItem *item = findItem(id);
    if (item) {
        QString name = item->data(Qt::UserRole + 1).toString();
        updateItemDisplay(item, id, name);
    }
}

void ConnectionPanel::clearUnreadCount(int id)
{
    setUnreadCount(id, 0);
}

int ConnectionPanel::selectedConnectionId() const
{
    QListWidgetItem *item = m_listWidget->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void ConnectionPanel::updateItemDisplay(QListWidgetItem *item, int connectionId, const QString &name)
{
    bool connected = m_connectedState.value(connectionId, false);
    bool loading   = m_loadingState.value(connectionId, false);

    QString prefix;
    QColor color;
    if (loading) {
        int frame = m_spinnerFrame.value(connectionId, 0);
        prefix = QString::fromUtf8(kSpinnerFrames[frame % kSpinnerFrameCount]) + " ";
        color  = QColor("#f39800");
    } else if (connected) {
        prefix = QString::fromUtf8("\u25CF") + " "; // ●
        color  = QColor("#4caf50");
    } else {
        prefix = QString::fromUtf8("\u25CB") + " "; // ○
        color  = QColor("#a0a0b0");
    }

    int unread = m_unreadCount.value(connectionId, 0);
    QString badge = (unread > 0) ? QString("  [%1]").arg(unread) : QString();

    item->setText(prefix + name + badge);
    item->setForeground(color);
    item->setToolTip(loading ? "连接中..." : (connected ? "已连接" : "未连接"));
}

void ConnectionPanel::onSpinnerTick()
{
    for (auto it = m_loadingState.begin(); it != m_loadingState.end(); ++it) {
        if (!it.value()) continue;
        int id = it.key();
        m_spinnerFrame[id] = (m_spinnerFrame.value(id, 0) + 1) % kSpinnerFrameCount;
        QListWidgetItem *item = findItem(id);
        if (item) {
            QString name = item->data(Qt::UserRole + 1).toString();
            updateItemDisplay(item, id, name);
        }
    }
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

bool ConnectionPanel::hasAnyLoading() const
{
    for (bool v : m_loadingState)
        if (v) return true;
    return false;
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
