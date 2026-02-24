#ifndef CONNECTIONPANEL_H
#define CONNECTIONPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include "core/models.h"

class ConnectionPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    void addConnection(const MqttConnectionConfig &config, bool connected = false);
    void updateConnection(const MqttConnectionConfig &config);
    void removeConnection(int id);
    void setConnected(int id, bool connected);
    void clearConnections();

    int selectedConnectionId() const;

signals:
    void connectRequested(int connectionId);
    void disconnectRequested(int connectionId);
    void editRequested(int connectionId);
    void deleteRequested(int connectionId);
    void addRequested();
    void selectionChanged(int connectionId);

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);

private:
    QListWidget *m_listWidget;
    QMap<int, bool> m_connectedState; // connectionId -> isConnected

    void updateItemDisplay(QListWidgetItem *item, int connectionId, const QString &name);
    QListWidgetItem *findItem(int connectionId) const;
};

#endif // CONNECTIONPANEL_H
