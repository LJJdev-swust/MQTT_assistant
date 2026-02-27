#ifndef SUBSCRIPTIONPANEL_H
#define SUBSCRIPTIONPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include "core/models.h"

class SubscriptionPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SubscriptionPanel(QWidget *parent = nullptr);

    void loadSubscriptions(const QList<SubscriptionConfig> &subs);
    void addSubscription(const SubscriptionConfig &sub);
    void removeSubscriptionById(int id);
    void clearSubscriptions();

    QList<SubscriptionConfig> subscriptions() const;

signals:
    void addRequested();
    void unsubscribeRequested(const QString &topic, int id);
    void copyTopicRequested(const QString &topic);

private slots:
    void onContextMenu(const QPoint &pos);

private:
    QListWidget *m_listWidget;
    QMap<int, SubscriptionConfig> m_subs; // id -> config
    QListWidgetItem *findItem(int id) const;
};

#endif // SUBSCRIPTIONPANEL_H
