#ifndef CONNECTIONPANEL_H
#define CONNECTIONPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QMap>
#include <QTimer>
#include "core/models.h"

class ConnectionPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectionPanel(QWidget *parent = nullptr);
    ~ConnectionPanel();

    void addConnection(const MqttConnectionConfig &config, bool connected = false);
    void updateConnection(const MqttConnectionConfig &config);
    void removeConnection(int id);
    void setConnected(int id, bool connected);
    void setLoading(int id, bool loading);
    void setUnreadCount(int id, int count);
    void clearUnreadCount(int id);
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
    void onSpinnerTick();

private:
    QListWidget *m_listWidget;
    QMap<int, bool>    m_connectedState;  // connectionId -> isConnected
    QMap<int, bool>    m_loadingState;    // connectionId -> isLoading
    QMap<int, int>     m_spinnerFrame;    // connectionId -> frame index
    QMap<int, int>     m_unreadCount;     // connectionId -> unread message count
    QTimer            *m_spinnerTimer;

    static const char *kSpinnerFrames[];
    static const int   kSpinnerFrameCount;

    void updateItemDisplay(QListWidgetItem *item, int connectionId, const QString &name);
    QListWidgetItem *findItem(int connectionId) const;
    bool hasAnyLoading() const;
};

#endif // CONNECTIONPANEL_H
