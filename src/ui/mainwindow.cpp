#include "mainwindow.h"
#include "dialogs/connectiondialog.h"
#include "dialogs/commanddialog.h"
#include "dialogs/scriptdialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QHeaderView>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QDateTime>
#include <QLabel>
#include <QFrame>
#include <QListWidgetItem>

// ──────────────────────────────────────────────
//  Construction
// ──────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_activeConnectionId(-1)
{
    setWindowTitle("MQTT Assistant");
    setMinimumSize(900, 620);
    resize(1100, 720);

    if (!m_db.open())
        QMessageBox::critical(this, "Database Error", "Failed to open the database.");

    setupMenuBar();
    setupUi();
    loadAllData();
}

MainWindow::~MainWindow()
{
    // Disconnect all active clients
    for (MqttClient *client : m_clients.values()) {
        if (client->isConnected())
            client->disconnectFromHost();
        client->deleteLater();
    }
}

// ──────────────────────────────────────────────
//  UI Setup
// ──────────────────────────────────────────────

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *hLayout = new QHBoxLayout(central);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    // Sidebar
    QWidget *sidebar = new QWidget(central);
    sidebar->setObjectName("sidebarWidget");
    sidebar->setFixedWidth(220);
    setupSidebar(sidebar);
    hLayout->addWidget(sidebar);

    // Content
    QWidget *content = new QWidget(central);
    setupContentArea(content);
    hLayout->addWidget(content, 1);

    // Status bar
    m_statusLabel = new QLabel("Not connected", this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupSidebar(QWidget *sidebar)
{
    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(6);

    // App title
    QLabel *titleLabel = new QLabel("MQTT Assistant", sidebar);
    titleLabel->setObjectName("labelAppTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // Separator
    QFrame *sep = new QFrame(sidebar);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #333350;");
    layout->addWidget(sep);

    // ---- Connections section ----
    QHBoxLayout *connHeader = new QHBoxLayout();
    QLabel *connLabel = new QLabel("CONNECTIONS", sidebar);
    connLabel->setObjectName("labelSectionConnections");
    QPushButton *addConnBtn = new QPushButton("+", sidebar);
    addConnBtn->setObjectName("btnAddConnection");
    addConnBtn->setFixedSize(24, 24);
    addConnBtn->setToolTip("Add Connection");
    connHeader->addWidget(connLabel);
    connHeader->addStretch();
    connHeader->addWidget(addConnBtn);
    layout->addLayout(connHeader);

    m_connectionPanel = new ConnectionPanel(sidebar);
    m_connectionPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_connectionPanel->setMinimumHeight(100);
    m_connectionPanel->setMaximumHeight(180);
    layout->addWidget(m_connectionPanel);

    // ---- Commands section ----
    QHBoxLayout *cmdHeader = new QHBoxLayout();
    QLabel *cmdLabel = new QLabel("COMMANDS", sidebar);
    cmdLabel->setObjectName("labelSectionCommands");
    QPushButton *addCmdBtn = new QPushButton("+", sidebar);
    addCmdBtn->setObjectName("btnAddCommand");
    addCmdBtn->setFixedSize(24, 24);
    addCmdBtn->setToolTip("Add Command");
    cmdHeader->addWidget(cmdLabel);
    cmdHeader->addStretch();
    cmdHeader->addWidget(addCmdBtn);
    layout->addLayout(cmdHeader);

    m_commandPanel = new CommandPanel(sidebar);
    m_commandPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_commandPanel->setMinimumHeight(80);
    m_commandPanel->setMaximumHeight(160);
    layout->addWidget(m_commandPanel);

    // ---- Scripts section ----
    QHBoxLayout *scriptHeader = new QHBoxLayout();
    QLabel *scriptLabel = new QLabel("SCRIPTS", sidebar);
    scriptLabel->setObjectName("labelSectionScripts");
    QPushButton *addScriptBtn = new QPushButton("+", sidebar);
    addScriptBtn->setObjectName("btnAddScript");
    addScriptBtn->setFixedSize(24, 24);
    addScriptBtn->setToolTip("Add Script");
    scriptHeader->addWidget(scriptLabel);
    scriptHeader->addStretch();
    scriptHeader->addWidget(addScriptBtn);
    layout->addLayout(scriptHeader);

    m_scriptList = new QListWidget(sidebar);
    m_scriptList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_scriptList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_scriptList, 1);

    // Wire up signals
    connect(addConnBtn,   &QPushButton::clicked, this, &MainWindow::onAddConnection);
    connect(addCmdBtn,    &QPushButton::clicked, this, &MainWindow::onAddCommand);
    connect(addScriptBtn, &QPushButton::clicked, this, &MainWindow::onAddScript);

    connect(m_connectionPanel, &ConnectionPanel::addRequested,
            this, &MainWindow::onAddConnection);
    connect(m_connectionPanel, &ConnectionPanel::editRequested,
            this, &MainWindow::onEditConnection);
    connect(m_connectionPanel, &ConnectionPanel::deleteRequested,
            this, &MainWindow::onDeleteConnection);
    connect(m_connectionPanel, &ConnectionPanel::connectRequested,
            this, &MainWindow::onConnectRequested);
    connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
            this, &MainWindow::onDisconnectRequested);
    connect(m_connectionPanel, &ConnectionPanel::selectionChanged,
            this, &MainWindow::onConnectionSelectionChanged);

    connect(m_commandPanel, &CommandPanel::editRequested,
            this, &MainWindow::onEditCommand);
    connect(m_commandPanel, &CommandPanel::deleteRequested,
            this, &MainWindow::onDeleteCommand);
    connect(m_commandPanel, &CommandPanel::addRequested,
            this, &MainWindow::onAddCommand);

    connect(m_scriptList, &QListWidget::customContextMenuRequested,
            [this](const QPoint &pos) {
                QListWidgetItem *item = m_scriptList->itemAt(pos);
                if (!item) return;
                int id = item->data(Qt::UserRole).toInt();
                QMenu menu(this);
                QAction *actEdit   = menu.addAction("Edit");
                QAction *actDelete = menu.addAction("Delete");
                QAction *chosen = menu.exec(m_scriptList->viewport()->mapToGlobal(pos));
                if (chosen == actEdit)   onEditScript(id);
                if (chosen == actDelete) onDeleteScript(id);
            });

    connect(m_scriptList, &QListWidget::itemChanged,
            this, &MainWindow::onScriptItemChanged);
}

void MainWindow::setupContentArea(QWidget *content)
{
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(content);

    // Chat tab
    m_chatWidget = new ChatWidget(content);
    m_tabWidget->addTab(m_chatWidget, "Chat");

    connect(m_chatWidget, &ChatWidget::sendRequested,
            this, &MainWindow::onSendRequested);
    connect(m_chatWidget, &ChatWidget::subscribeRequested,
            this, &MainWindow::onSubscribeRequested);

    // Monitor tab
    QWidget *monitorWidget = new QWidget(content);
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorWidget);
    monitorLayout->setContentsMargins(4, 4, 4, 4);

    m_monitorTable = new QTableWidget(0, 4, monitorWidget);
    m_monitorTable->setHorizontalHeaderLabels({"Time", "Direction", "Topic", "Payload"});
    m_monitorTable->horizontalHeader()->setStretchLastSection(true);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_monitorTable->setColumnWidth(0, 85);
    m_monitorTable->setColumnWidth(1, 75);
    m_monitorTable->setColumnWidth(2, 200);
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitorTable->setAlternatingRowColors(false);
    m_monitorTable->verticalHeader()->setVisible(false);
    monitorLayout->addWidget(m_monitorTable);

    m_tabWidget->addTab(monitorWidget, "Monitor");

    layout->addWidget(m_tabWidget);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *mb = menuBar();

    QMenu *fileMenu = mb->addMenu("File");
    QAction *actQuit = fileMenu->addAction("Quit");
    connect(actQuit, &QAction::triggered, this, &QMainWindow::close);

    QMenu *connMenu = mb->addMenu("Connections");
    QAction *actAddConn = connMenu->addAction("New Connection...");
    connect(actAddConn, &QAction::triggered, this, &MainWindow::onAddConnection);

    QMenu *helpMenu = mb->addMenu("Help");
    QAction *actAbout = helpMenu->addAction("About");
    connect(actAbout, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About MQTT Assistant",
            "<b>MQTT Assistant</b><br>A Qt5 MQTT client application.<br><br>"
            "Built with Qt " QT_VERSION_STR);
    });
}

// ──────────────────────────────────────────────
//  Data loading
// ──────────────────────────────────────────────

void MainWindow::loadAllData()
{
    // Connections
    QList<MqttConnectionConfig> conns = m_db.loadConnections();
    for (const MqttConnectionConfig &c : conns) {
        m_connections[c.id] = c;
        m_connectionPanel->addConnection(c, false);
    }

    // Commands
    QList<CommandConfig> cmds = m_db.loadCommands();
    for (const CommandConfig &cmd : cmds) {
        m_commands[cmd.id] = cmd;
        m_commandPanel->addCommand(cmd);
    }

    // Scripts
    QList<ScriptConfig> scripts = m_db.loadScripts();
    for (const ScriptConfig &s : scripts) {
        m_scripts[s.id] = s;
    }
    m_scriptEngine.setScripts(scripts);
    refreshScriptList();
}

void MainWindow::refreshScriptList()
{
    // Block signals while rebuilding to avoid spurious itemChanged
    m_scriptList->blockSignals(true);
    m_scriptList->clear();
    for (const ScriptConfig &s : m_scripts.values()) {
        QListWidgetItem *item = new QListWidgetItem(s.name);
        item->setData(Qt::UserRole, s.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(s.enabled ? Qt::Checked : Qt::Unchecked);
        m_scriptList->addItem(item);
    }
    m_scriptList->blockSignals(false);
}

// ──────────────────────────────────────────────
//  Connection Panel Slots
// ──────────────────────────────────────────────

void MainWindow::onAddConnection()
{
    ConnectionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    MqttConnectionConfig config = dlg.config();
    if (config.name.isEmpty()) {
        QMessageBox::warning(this, "Invalid", "Connection name cannot be empty.");
        return;
    }
    int id = m_db.saveConnection(config);
    if (id < 0) {
        QMessageBox::critical(this, "Error", "Failed to save connection.");
        return;
    }
    config.id = id;
    m_connections[id] = config;
    m_connectionPanel->addConnection(config, false);
}

void MainWindow::onEditConnection(int connectionId)
{
    if (!m_connections.contains(connectionId)) return;
    MqttConnectionConfig config = m_connections[connectionId];
    ConnectionDialog dlg(config, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    MqttConnectionConfig updated = dlg.config();
    updated.id = connectionId;
    m_db.updateConnection(updated);
    m_connections[connectionId] = updated;
    m_connectionPanel->updateConnection(updated);
}

void MainWindow::onDeleteConnection(int connectionId)
{
    int ret = QMessageBox::question(this, "Delete Connection",
        "Delete this connection and all its messages?",
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // Disconnect if active
    if (m_clients.contains(connectionId)) {
        m_clients[connectionId]->disconnectFromHost();
        m_clients[connectionId]->deleteLater();
        m_clients.remove(connectionId);
    }
    m_db.deleteMessages(connectionId);
    m_db.deleteConnection(connectionId);
    m_connections.remove(connectionId);
    m_connectionPanel->removeConnection(connectionId);

    if (m_activeConnectionId == connectionId) {
        m_activeConnectionId = -1;
        setWindowTitle("MQTT Assistant");
        m_statusLabel->setText("Not connected");
    }
}

void MainWindow::onConnectRequested(int connectionId)
{
    if (!m_connections.contains(connectionId)) return;
    const MqttConnectionConfig &config = m_connections[connectionId];

    // Create client if needed
    if (!m_clients.contains(connectionId)) {
        MqttClient *client = new MqttClient(this);
        m_clients[connectionId] = client;

    connect(client, &MqttClient::connected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, true);
            if (m_activeConnectionId == connectionId) {
                const QString &name = m_connections.value(connectionId).name;
                setWindowTitle("MQTT Assistant - " + name);
                m_statusLabel->setText("Connected to " + name);
            }
        });
        connect(client, &MqttClient::disconnected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, false);
            if (m_activeConnectionId == connectionId) {
                setWindowTitle("MQTT Assistant");
                m_statusLabel->setText("Disconnected");
            }
        });
        connect(client, &MqttClient::messageReceived, this,
                [this, connectionId](const QString &topic, const QString &payload) {
                    if (m_activeConnectionId == connectionId)
                        saveAndDisplayMessage(topic, payload, false, connectionId);
                });
        connect(client, &MqttClient::errorOccurred, this,
                [this](const QString &msg) {
                    statusBar()->showMessage("Error: " + msg, 5000);
                });
    }

    // Wire script engine to this client if it's the active one
    // (script engine tracks one client)
    MqttClient *client = m_clients[connectionId];

    // Disconnect existing signal connections to avoid re-connections from before
    // Reconnect connected/disconnected so we update the panel correctly
    // (Already wired above per-client – the lambda captures ensure the right id)

    client->connectToHost(config);

    // Set active connection on connect
    m_activeConnectionId = connectionId;

    // Update script engine
    m_scriptEngine.setClient(client);

    // Filter scripts for this connection
    QList<ScriptConfig> connScripts;
    for (const ScriptConfig &s : m_scripts.values()) {
        if (s.connectionId == connectionId || s.connectionId == -1)
            connScripts.append(s);
    }
    m_scriptEngine.setScripts(connScripts);

    // Update command panel client
    m_commandPanel->setClient(client);
    m_chatWidget->setClient(client);

    // Load message history
    QList<MessageRecord> history = m_db.loadMessages(connectionId, 100);
    m_chatWidget->loadMessages(history);

    // Also populate monitor table
    m_monitorTable->setRowCount(0);
    for (const MessageRecord &msg : history)
        addMessageToMonitor(msg);
}

void MainWindow::onDisconnectRequested(int connectionId)
{
    if (!m_clients.contains(connectionId)) return;
    m_clients[connectionId]->disconnectFromHost();
}

void MainWindow::onConnectionSelectionChanged(int connectionId)
{
    if (connectionId == m_activeConnectionId) return;

    // Just switch display – don't connect/disconnect
    m_activeConnectionId = connectionId;

    if (m_connections.contains(connectionId)) {
        bool isConn = m_clients.contains(connectionId) &&
                      m_clients[connectionId]->isConnected();

        if (isConn) {
            const QString &name = m_connections[connectionId].name;
            setWindowTitle("MQTT Assistant - " + name);
            m_statusLabel->setText("Connected to " + name);
            m_commandPanel->setClient(m_clients[connectionId]);
            m_chatWidget->setClient(m_clients[connectionId]);
        } else {
            setWindowTitle("MQTT Assistant");
            m_statusLabel->setText("Not connected");
            m_commandPanel->setClient(nullptr);
            m_chatWidget->setClient(nullptr);
        }

        // Load message history
        QList<MessageRecord> history = m_db.loadMessages(connectionId, 100);
        m_chatWidget->loadMessages(history);
        m_monitorTable->setRowCount(0);
        for (const MessageRecord &msg : history)
            addMessageToMonitor(msg);
    }
}

// ──────────────────────────────────────────────
//  Command Panel Slots
// ──────────────────────────────────────────────

void MainWindow::onAddCommand()
{
    CommandDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    CommandConfig cmd = dlg.config();
    if (cmd.name.isEmpty()) {
        QMessageBox::warning(this, "Invalid", "Command name cannot be empty.");
        return;
    }
    cmd.connectionId = m_activeConnectionId;
    int id = m_db.saveCommand(cmd);
    if (id < 0) {
        QMessageBox::critical(this, "Error", "Failed to save command.");
        return;
    }
    cmd.id = id;
    m_commands[id] = cmd;
    m_commandPanel->addCommand(cmd);
}

void MainWindow::onEditCommand(int commandId)
{
    if (!m_commands.contains(commandId)) return;
    CommandDialog dlg(m_commands[commandId], this);
    if (dlg.exec() != QDialog::Accepted) return;
    CommandConfig updated = dlg.config();
    updated.id           = commandId;
    updated.connectionId = m_commands[commandId].connectionId;
    m_db.updateCommand(updated);
    m_commands[commandId] = updated;
    m_commandPanel->updateCommand(updated);
}

void MainWindow::onDeleteCommand(int commandId)
{
    int ret = QMessageBox::question(this, "Delete Command",
        "Delete this command?", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_db.deleteCommand(commandId);
    m_commands.remove(commandId);
    m_commandPanel->removeCommand(commandId);
}

// ──────────────────────────────────────────────
//  Script List Slots
// ──────────────────────────────────────────────

void MainWindow::onAddScript()
{
    ScriptDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    ScriptConfig script = dlg.config();
    if (script.name.isEmpty()) {
        QMessageBox::warning(this, "Invalid", "Script name cannot be empty.");
        return;
    }
    script.connectionId = m_activeConnectionId;
    int id = m_db.saveScript(script);
    if (id < 0) {
        QMessageBox::critical(this, "Error", "Failed to save script.");
        return;
    }
    script.id = id;
    m_scripts[id] = script;
    m_scriptEngine.addScript(script);
    refreshScriptList();
}

void MainWindow::onEditScript(int scriptId)
{
    if (!m_scripts.contains(scriptId)) return;
    ScriptDialog dlg(m_scripts[scriptId], this);
    if (dlg.exec() != QDialog::Accepted) return;
    ScriptConfig updated     = dlg.config();
    updated.id               = scriptId;
    updated.connectionId     = m_scripts[scriptId].connectionId;
    m_db.updateScript(updated);
    m_scripts[scriptId]      = updated;
    m_scriptEngine.updateScript(updated);
    refreshScriptList();
}

void MainWindow::onDeleteScript(int scriptId)
{
    int ret = QMessageBox::question(this, "Delete Script",
        "Delete this script?", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_db.deleteScript(scriptId);
    m_scripts.remove(scriptId);
    m_scriptEngine.removeScript(scriptId);
    refreshScriptList();
}

void MainWindow::onScriptItemChanged(QListWidgetItem *item)
{
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    if (!m_scripts.contains(id)) return;
    bool enabled = (item->checkState() == Qt::Checked);
    m_scripts[id].enabled = enabled;
    m_db.updateScript(m_scripts[id]);
    m_scriptEngine.updateScript(m_scripts[id]);
}

// ──────────────────────────────────────────────
//  Chat Slots
// ──────────────────────────────────────────────

void MainWindow::onSendRequested(const QString &topic, const QString &payload)
{
    if (m_activeConnectionId < 0 || !m_clients.contains(m_activeConnectionId)) {
        QMessageBox::warning(this, "Not Connected", "Please connect first.");
        return;
    }
    MqttClient *client = m_clients[m_activeConnectionId];
    if (!client->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect first.");
        return;
    }
    client->publish(topic, payload);
    saveAndDisplayMessage(topic, payload, true, m_activeConnectionId);
}

void MainWindow::onSubscribeRequested(const QString &topic)
{
    if (m_activeConnectionId < 0 || !m_clients.contains(m_activeConnectionId)) {
        QMessageBox::warning(this, "Not Connected", "Please connect first.");
        return;
    }
    MqttClient *client = m_clients[m_activeConnectionId];
    if (!client->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Please connect first.");
        return;
    }
    client->subscribe(topic, 0);
    statusBar()->showMessage("Subscribed to: " + topic, 3000);
}

// ──────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────

void MainWindow::saveAndDisplayMessage(const QString &topic, const QString &payload,
                                       bool outgoing, int connectionId)
{
    MessageRecord msg;
    msg.connectionId = connectionId;
    msg.topic        = topic;
    msg.payload      = payload;
    msg.outgoing     = outgoing;
    msg.timestamp    = QDateTime::currentDateTime();

    int id = m_db.saveMessage(msg);
    msg.id = id;

    m_chatWidget->addMessage(msg);
    addMessageToMonitor(msg);
}

void MainWindow::addMessageToMonitor(const MessageRecord &msg)
{
    int row = m_monitorTable->rowCount();
    m_monitorTable->insertRow(row);

    m_monitorTable->setItem(row, 0, new QTableWidgetItem(
        msg.timestamp.toString("hh:mm:ss")));
    m_monitorTable->setItem(row, 1, new QTableWidgetItem(
        msg.outgoing ? "↑ Sent" : "↓ Recv"));
    m_monitorTable->setItem(row, 2, new QTableWidgetItem(msg.topic));
    m_monitorTable->setItem(row, 3, new QTableWidgetItem(msg.payload));

    // Colour code direction
    QColor dirColor = msg.outgoing ? QColor("#ea5413") : QColor("#4caf50");
    m_monitorTable->item(row, 1)->setForeground(dirColor);

    m_monitorTable->scrollToBottom();
}

MqttClient *MainWindow::clientForId(int connectionId)
{
    return m_clients.value(connectionId, nullptr);
}

MqttConnectionConfig MainWindow::configForId(int connectionId) const
{
    return m_connections.value(connectionId, MqttConnectionConfig());
}

CommandConfig MainWindow::commandConfigForId(int commandId) const
{
    return m_commands.value(commandId, CommandConfig());
}

ScriptConfig MainWindow::scriptConfigForId(int scriptId) const
{
    return m_scripts.value(scriptId, ScriptConfig());
}
