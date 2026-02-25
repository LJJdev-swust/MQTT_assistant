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
#include <QInputDialog>
#include <QResizeEvent>
#include <QFileDialog>
#include <QSettings>
#include <QCoreApplication>

// ──────────────────────────────────────────────
//  Construction
// ──────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_activeConnectionId(-1)
    , m_toastLabel(nullptr)
    , m_toastTimer(nullptr)
{
    setWindowTitle("MQTT 助手");
    setMinimumSize(960, 640);
    resize(1200, 780);

    // Determine the database directory, prompting the user on first run or if unset
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    QString dbDir = settings.value("database/directory").toString();
    if (dbDir.isEmpty()) {
        QString defaultDir = QCoreApplication::applicationDirPath();
        dbDir = QFileDialog::getExistingDirectory(
            nullptr,
            "选择数据库存储路径",
            defaultDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (dbDir.isEmpty())
            dbDir = defaultDir;
        settings.setValue("database/directory", dbDir);
    }

    if (!m_db.open(dbDir + "/mqtt_assistant.db"))
        QMessageBox::critical(this, "数据库错误", "无法打开数据库，请检查存储权限。");

    setupMenuBar();
    setupUi();
    loadAllData();
}

MainWindow::~MainWindow()
{
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
    sidebar->setFixedWidth(240);
    setupSidebar(sidebar);
    hLayout->addWidget(sidebar);

    // Content
    QWidget *content = new QWidget(central);
    setupContentArea(content);
    hLayout->addWidget(content, 1);

    // Status bar
    m_statusLabel = new QLabel("未连接", this);
    statusBar()->addWidget(m_statusLabel);

    // Toast overlay
    m_toastLabel = new QLabel(this);
    m_toastLabel->setObjectName("toastLabel");
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->setWordWrap(true);
    m_toastLabel->hide();

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toastLabel, &QLabel::hide);
}

void MainWindow::setupSidebar(QWidget *sidebar)
{
    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(6);

    // App title
    QLabel *titleLabel = new QLabel("MQTT 助手", sidebar);
    titleLabel->setObjectName("labelAppTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // Separator
    QFrame *sep = new QFrame(sidebar);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    layout->addWidget(sep);

    // ---- Connections section ----
    QHBoxLayout *connHeader = new QHBoxLayout();
    QLabel *connLabel = new QLabel("连接管理", sidebar);
    connLabel->setObjectName("labelSectionConnections");
    QPushButton *addConnBtn = new QPushButton("+", sidebar);
    addConnBtn->setObjectName("btnAddConnection");
    addConnBtn->setFixedSize(24, 24);
    addConnBtn->setToolTip("新建连接");
    connHeader->addWidget(connLabel);
    connHeader->addStretch();
    connHeader->addWidget(addConnBtn);
    layout->addLayout(connHeader);

    m_connectionPanel = new ConnectionPanel(sidebar);
    m_connectionPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_connectionPanel->setMinimumHeight(80);
    m_connectionPanel->setMaximumHeight(160);
    layout->addWidget(m_connectionPanel);

    // ---- Subscriptions section ----
    QHBoxLayout *subHeader = new QHBoxLayout();
    QLabel *subLabel = new QLabel("订阅管理", sidebar);
    subLabel->setObjectName("labelSectionSubscriptions");
    QPushButton *addSubBtn = new QPushButton("+", sidebar);
    addSubBtn->setObjectName("btnAddSubscription");
    addSubBtn->setFixedSize(24, 24);
    addSubBtn->setToolTip("新增订阅");
    subHeader->addWidget(subLabel);
    subHeader->addStretch();
    subHeader->addWidget(addSubBtn);
    layout->addLayout(subHeader);

    m_subscriptionPanel = new SubscriptionPanel(sidebar);
    m_subscriptionPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_subscriptionPanel->setMinimumHeight(60);
    m_subscriptionPanel->setMaximumHeight(140);
    layout->addWidget(m_subscriptionPanel);

    // ---- Commands section ----
    QHBoxLayout *cmdHeader = new QHBoxLayout();
    QLabel *cmdLabel = new QLabel("命令", sidebar);
    cmdLabel->setObjectName("labelSectionCommands");
    QPushButton *addCmdBtn = new QPushButton("+", sidebar);
    addCmdBtn->setObjectName("btnAddCommand");
    addCmdBtn->setFixedSize(24, 24);
    addCmdBtn->setToolTip("新建命令");
    cmdHeader->addWidget(cmdLabel);
    cmdHeader->addStretch();
    cmdHeader->addWidget(addCmdBtn);
    layout->addLayout(cmdHeader);

    m_commandPanel = new CommandPanel(sidebar);
    m_commandPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_commandPanel->setMinimumHeight(60);
    m_commandPanel->setMaximumHeight(140);
    layout->addWidget(m_commandPanel);

    // ---- Scripts section ----
    QHBoxLayout *scriptHeader = new QHBoxLayout();
    QLabel *scriptLabel = new QLabel("脚本", sidebar);
    scriptLabel->setObjectName("labelSectionScripts");
    QPushButton *addScriptBtn = new QPushButton("+", sidebar);
    addScriptBtn->setObjectName("btnAddScript");
    addScriptBtn->setFixedSize(24, 24);
    addScriptBtn->setToolTip("新建脚本");
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
    connect(addSubBtn,    &QPushButton::clicked, this, &MainWindow::onAddSubscription);
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

    connect(m_subscriptionPanel, &SubscriptionPanel::addRequested,
            this, &MainWindow::onAddSubscription);
    connect(m_subscriptionPanel, &SubscriptionPanel::unsubscribeRequested,
            this, &MainWindow::onUnsubscribeRequested);

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
                QAction *actEdit   = menu.addAction("编辑");
                QAction *actDelete = menu.addAction("删除");
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
    m_tabWidget->addTab(m_chatWidget, "消息");

    connect(m_chatWidget, &ChatWidget::sendRequested,
            this, &MainWindow::onSendRequested);
    connect(m_chatWidget, &ChatWidget::subscribeRequested,
            this, &MainWindow::onSubscribeRequested);

    // Monitor tab
    QWidget *monitorWidget = new QWidget(content);
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorWidget);
    monitorLayout->setContentsMargins(4, 4, 4, 4);

    m_monitorTable = new QTableWidget(0, 4, monitorWidget);
    m_monitorTable->setHorizontalHeaderLabels({"时间", "方向", "主题", "内容"});
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

    m_tabWidget->addTab(monitorWidget, "监控");

    layout->addWidget(m_tabWidget);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *mb = menuBar();

    QMenu *fileMenu = mb->addMenu("文件");
    QAction *actQuit = fileMenu->addAction("退出");
    connect(actQuit, &QAction::triggered, this, &QMainWindow::close);

    QMenu *connMenu = mb->addMenu("连接");
    QAction *actAddConn = connMenu->addAction("新建连接...");
    connect(actAddConn, &QAction::triggered, this, &MainWindow::onAddConnection);

    QMenu *helpMenu = mb->addMenu("帮助");
    QAction *actAbout = helpMenu->addAction("关于");
    connect(actAbout, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 MQTT 助手",
            "<b>MQTT 助手</b><br>基于 Qt6 的 MQTT 客户端工具。<br><br>"
            "编译版本：Qt " QT_VERSION_STR);
    });
}

// ──────────────────────────────────────────────
//  Toast Notification
// ──────────────────────────────────────────────

void MainWindow::showToast(const QString &message, int durationMs)
{
    m_toastLabel->setText(message);
    m_toastLabel->setFixedWidth(qMin(width() - 40, 380));
    m_toastLabel->adjustSize();
    int x = (width() - m_toastLabel->width()) / 2;
    int y = height() - m_toastLabel->height() - 70;
    m_toastLabel->move(x, y);
    m_toastLabel->show();
    m_toastLabel->raise();
    m_toastTimer->stop();
    m_toastTimer->start(durationMs);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_toastLabel && m_toastLabel->isVisible()) {
        int x = (width() - m_toastLabel->width()) / 2;
        int y = height() - m_toastLabel->height() - 70;
        m_toastLabel->move(x, y);
    }
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
//  Subscription helpers
// ──────────────────────────────────────────────

void MainWindow::subscribeAllForConnection(int connectionId)
{
    if (!m_clients.contains(connectionId)) return;
    MqttClient *client = m_clients[connectionId];
    if (!client->isConnected()) return;
    QList<SubscriptionConfig> subs = m_db.loadSubscriptions(connectionId);
    for (const SubscriptionConfig &s : subs)
        client->subscribe(s.topic, s.qos);
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
        showToast("连接名称不能为空");
        return;
    }
    int id = m_db.saveConnection(config);
    if (id < 0) {
        showToast("保存连接失败");
        return;
    }
    config.id = id;
    m_connections[id] = config;
    m_connectionPanel->addConnection(config, false);
    showToast("连接已添加：" + config.name);
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
    showToast("连接已更新：" + updated.name);
}

void MainWindow::onDeleteConnection(int connectionId)
{
    int ret = QMessageBox::question(this, "删除连接",
        "确定要删除此连接及其所有消息记录吗？",
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

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
        setWindowTitle("MQTT 助手");
        m_statusLabel->setText("未连接");
        m_subscriptionPanel->clearSubscriptions();
    }
    showToast("连接已删除");
}

void MainWindow::onConnectRequested(int connectionId)
{
    if (!m_connections.contains(connectionId)) return;
    const MqttConnectionConfig &config = m_connections[connectionId];

    if (!m_clients.contains(connectionId)) {
        MqttClient *client = new MqttClient(this);
        m_clients[connectionId] = client;

        connect(client, &MqttClient::connected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, true);
            if (m_activeConnectionId == connectionId) {
                const QString &name = m_connections.value(connectionId).name;
                setWindowTitle("MQTT 助手 - " + name);
                m_statusLabel->setText("已连接：" + name);
                showToast("已连接到 " + name);
            }
            // Auto-subscribe to saved subscriptions
            subscribeAllForConnection(connectionId);
        });
        connect(client, &MqttClient::disconnected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, false);
            if (m_activeConnectionId == connectionId) {
                setWindowTitle("MQTT 助手");
                m_statusLabel->setText("已断开连接");
                showToast("已断开连接");
            }
        });
        connect(client, &MqttClient::messageReceived, this,
                [this, connectionId](const QString &topic, const QString &payload) {
                    if (m_activeConnectionId == connectionId)
                        saveAndDisplayMessage(topic, payload, false, connectionId);
                });
        connect(client, &MqttClient::errorOccurred, this,
                [this](const QString &msg) {
                    showToast("错误：" + msg, 4000);
                });
    }

    MqttClient *client = m_clients[connectionId];
    client->connectToHost(config);

    m_activeConnectionId = connectionId;
    m_scriptEngine.setClient(client);

    QList<ScriptConfig> connScripts;
    for (const ScriptConfig &s : m_scripts.values()) {
        if (s.connectionId == connectionId || s.connectionId == -1)
            connScripts.append(s);
    }
    m_scriptEngine.setScripts(connScripts);

    m_commandPanel->setClient(client);
    m_chatWidget->setClient(client);

    // Load message history
    QList<MessageRecord> history = m_db.loadMessages(connectionId, 100);
    m_chatWidget->loadMessages(history);

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

    m_activeConnectionId = connectionId;

    if (m_connections.contains(connectionId)) {
        bool isConn = m_clients.contains(connectionId) &&
                      m_clients[connectionId]->isConnected();

        if (isConn) {
            const QString &name = m_connections[connectionId].name;
            setWindowTitle("MQTT 助手 - " + name);
            m_statusLabel->setText("已连接：" + name);
            m_commandPanel->setClient(m_clients[connectionId]);
            m_chatWidget->setClient(m_clients[connectionId]);
        } else {
            setWindowTitle("MQTT 助手");
            m_statusLabel->setText("未连接");
            m_commandPanel->setClient(nullptr);
            m_chatWidget->setClient(nullptr);
        }

        // Load subscriptions for this connection
        QList<SubscriptionConfig> subs = m_db.loadSubscriptions(connectionId);
        m_subscriptionPanel->loadSubscriptions(subs);

        // Load message history
        QList<MessageRecord> history = m_db.loadMessages(connectionId, 100);
        m_chatWidget->loadMessages(history);
        m_monitorTable->setRowCount(0);
        for (const MessageRecord &msg : history)
            addMessageToMonitor(msg);
    }
}

// ──────────────────────────────────────────────
//  Subscription Panel Slots
// ──────────────────────────────────────────────

void MainWindow::onAddSubscription()
{
    if (m_activeConnectionId < 0) {
        showToast("请先选择一个连接");
        return;
    }
    bool ok = false;
    QString topic = QInputDialog::getText(this, "新增订阅", "输入订阅主题（支持通配符 # 和 +）：",
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || topic.trimmed().isEmpty()) return;
    topic = topic.trimmed();

    SubscriptionConfig sub;
    sub.connectionId = m_activeConnectionId;
    sub.topic        = topic;
    sub.qos          = 0;
    int id = m_db.saveSubscription(sub);
    if (id < 0) {
        showToast("保存订阅失败");
        return;
    }
    sub.id = id;
    m_subscriptionPanel->addSubscription(sub);

    // Subscribe immediately if connected
    if (m_clients.contains(m_activeConnectionId) &&
        m_clients[m_activeConnectionId]->isConnected()) {
        m_clients[m_activeConnectionId]->subscribe(topic, 0);
    }
    showToast("已订阅：" + topic);
}

void MainWindow::onUnsubscribeRequested(const QString &topic, int id)
{
    m_db.deleteSubscription(id);
    m_subscriptionPanel->removeSubscriptionById(id);

    if (m_clients.contains(m_activeConnectionId) &&
        m_clients[m_activeConnectionId]->isConnected()) {
        m_clients[m_activeConnectionId]->unsubscribe(topic);
    }
    showToast("已取消订阅：" + topic);
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
        showToast("命令名称不能为空");
        return;
    }
    cmd.connectionId = m_activeConnectionId;
    int id = m_db.saveCommand(cmd);
    if (id < 0) {
        showToast("保存命令失败");
        return;
    }
    cmd.id = id;
    m_commands[id] = cmd;
    m_commandPanel->addCommand(cmd);
    showToast("命令已添加：" + cmd.name);
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
    showToast("命令已更新");
}

void MainWindow::onDeleteCommand(int commandId)
{
    int ret = QMessageBox::question(this, "删除命令",
        "确定要删除此命令吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_db.deleteCommand(commandId);
    m_commands.remove(commandId);
    m_commandPanel->removeCommand(commandId);
    showToast("命令已删除");
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
        showToast("脚本名称不能为空");
        return;
    }
    script.connectionId = m_activeConnectionId;
    int id = m_db.saveScript(script);
    if (id < 0) {
        showToast("保存脚本失败");
        return;
    }
    script.id = id;
    m_scripts[id] = script;
    m_scriptEngine.addScript(script);
    refreshScriptList();
    showToast("脚本已添加：" + script.name);
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
    showToast("脚本已更新");
}

void MainWindow::onDeleteScript(int scriptId)
{
    int ret = QMessageBox::question(this, "删除脚本",
        "确定要删除此脚本吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_db.deleteScript(scriptId);
    m_scripts.remove(scriptId);
    m_scriptEngine.removeScript(scriptId);
    refreshScriptList();
    showToast("脚本已删除");
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
        showToast("请先连接到 MQTT 服务器");
        return;
    }
    MqttClient *client = m_clients[m_activeConnectionId];
    if (!client->isConnected()) {
        showToast("请先连接到 MQTT 服务器");
        return;
    }
    client->publish(topic, payload);
    saveAndDisplayMessage(topic, payload, true, m_activeConnectionId);
}

void MainWindow::onSubscribeRequested(const QString &topic)
{
    if (m_activeConnectionId < 0 || !m_clients.contains(m_activeConnectionId)) {
        showToast("请先连接到 MQTT 服务器");
        return;
    }
    MqttClient *client = m_clients[m_activeConnectionId];
    if (!client->isConnected()) {
        showToast("请先连接到 MQTT 服务器");
        return;
    }
    client->subscribe(topic, 0);

    // Persist to DB if not already saved
    QList<SubscriptionConfig> existing = m_db.loadSubscriptions(m_activeConnectionId);
    bool alreadySaved = false;
    for (const SubscriptionConfig &s : existing) {
        if (s.topic == topic) { alreadySaved = true; break; }
    }
    if (!alreadySaved) {
        SubscriptionConfig sub;
        sub.connectionId = m_activeConnectionId;
        sub.topic        = topic;
        sub.qos          = 0;
        int id = m_db.saveSubscription(sub);
        if (id >= 0) {
            sub.id = id;
            m_subscriptionPanel->addSubscription(sub);
        }
    }
    showToast("已订阅：" + topic);
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
        msg.outgoing ? "↑ 发送" : "↓ 接收"));
    m_monitorTable->setItem(row, 2, new QTableWidgetItem(msg.topic));
    m_monitorTable->setItem(row, 3, new QTableWidgetItem(msg.payload));

    QColor dirColor = msg.outgoing ? QColor("#ea5413") : QColor("#1e9e50");
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
