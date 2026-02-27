#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QMap>
#include <QTimer>
#include <QThread>

#include "core/models.h"
#include "core/mqttclient.h"
#include "core/databasemanager.h"
#include "core/scriptengine.h"
#include "widgets/connectionpanel.h"
#include "widgets/commandpanel.h"
#include "widgets/chatwidget.h"
#include "widgets/subscriptionpanel.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Connection panel slots
    void onAddConnection();
    void onEditConnection(int connectionId);
    void onDeleteConnection(int connectionId);
    void onConnectRequested(int connectionId);
    void onDisconnectRequested(int connectionId);
    void onConnectionSelectionChanged(int connectionId);

    // Command panel slots
    void onAddCommand();
    void onEditCommand(int commandId);
    void onDeleteCommand(int commandId);

    // Script list slots
    void onAddScript();
    void onEditScript(int scriptId);
    void onDeleteScript(int scriptId);
    void onScriptItemChanged(QListWidgetItem *item);

    // Subscription panel slots
    void onAddSubscription();
    void onUnsubscribeRequested(const QString &topic, int id);

    // Chat widget slots
    void onSendRequested(const QString &topic, const QString &payload);
    void onSubscribeRequested(const QString &topic);
    void onClearHistoryRequested(int connectionId);

    // Monitor table
    void onMonitorRowDoubleClicked(int row, int col);

private:
    void setupUi();
    void setupSidebar(QWidget *sidebar);
    void setupContentArea(QWidget *content);
    void setupMenuBar();
    void loadAllData();
    void refreshCommandPanel(int connectionId);
    void refreshScriptList(int connectionId);
    void addMessageToMonitor(const MessageRecord &msg);
    void saveAndDisplayMessage(const QString &topic, const QString &payload,
                               bool outgoing, int connectionId, bool retained = false);
    void showToast(const QString &message, int durationMs = 2500);
    void subscribeAllForConnection(int connectionId);
    void updateSidebarTitle();
    void stopClientThread(int connectionId);

    bool initializeDatabase();
    bool promptForDatabasePath();
    void saveDatabasePathToSettings(const QString &path);
    QString loadDatabasePathFromSettings();

    MqttClient      *clientForId(int connectionId);
    MqttConnectionConfig configForId(int connectionId) const;
    CommandConfig    commandConfigForId(int commandId) const;
    ScriptConfig     scriptConfigForId(int scriptId) const;

    // Data
    DatabaseManager  m_db;
    QMap<int, MqttConnectionConfig> m_connections; // id -> config
    QMap<int, CommandConfig>        m_commands;    // id -> config
    QMap<int, ScriptConfig>         m_scripts;     // id -> config
    QMap<int, MqttClient*>          m_clients;     // connectionId -> client
    QMap<int, QThread*>             m_clientThreads; // connectionId -> thread
    QMap<int, int>                  m_unreadCounts;  // connectionId -> unread count
    ScriptEngine                    m_scriptEngine;

    int m_activeConnectionId;

    // UI widgets
    ConnectionPanel   *m_connectionPanel;
    SubscriptionPanel *m_subscriptionPanel;
    CommandPanel      *m_commandPanel;
    QListWidget       *m_scriptList;
    ChatWidget        *m_chatWidget;
    QTableWidget      *m_monitorTable;
    QTabWidget        *m_tabWidget;
    QLabel            *m_statusLabel;
    QLabel            *m_titleLabel;

    // Toast
    QLabel  *m_toastLabel;
    QTimer  *m_toastTimer;
};

#endif // MAINWINDOW_H
