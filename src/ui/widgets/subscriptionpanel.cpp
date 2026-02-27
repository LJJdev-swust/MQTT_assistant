#include "subscriptionpanel.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>

SubscriptionPanel::SubscriptionPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &SubscriptionPanel::onContextMenu);
}

void SubscriptionPanel::loadSubscriptions(const QList<SubscriptionConfig> &subs)
{
    clearSubscriptions();
    for (const SubscriptionConfig &s : subs)
        addSubscription(s);
}

void SubscriptionPanel::addSubscription(const SubscriptionConfig &sub)
{
    m_subs[sub.id] = sub;
    QString label = QString("[QoS%1] %2").arg(sub.qos).arg(sub.topic);
    QListWidgetItem *item = new QListWidgetItem(label, m_listWidget);
    item->setData(Qt::UserRole,     sub.id);
    item->setData(Qt::UserRole + 1, sub.topic);
    item->setToolTip(sub.topic);
    m_listWidget->addItem(item);
}

void SubscriptionPanel::removeSubscriptionById(int id)
{
    QListWidgetItem *item = findItem(id);
    if (item) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
        m_subs.remove(id);
    }
}

void SubscriptionPanel::clearSubscriptions()
{
    m_listWidget->clear();
    m_subs.clear();
}

QList<SubscriptionConfig> SubscriptionPanel::subscriptions() const
{
    return m_subs.values();
}

void SubscriptionPanel::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    int id    = item->data(Qt::UserRole).toInt();
    QString topic = item->data(Qt::UserRole + 1).toString();

    QMenu menu(this);
    QAction *actUnsub = menu.addAction("取消订阅");
    QAction *chosen = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (chosen == actUnsub)
        emit unsubscribeRequested(topic, id);
}

QListWidgetItem *SubscriptionPanel::findItem(int id) const
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == id)
            return item;
    }
    return nullptr;
}
