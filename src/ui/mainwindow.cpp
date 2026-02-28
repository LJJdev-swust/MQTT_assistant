#include "mainwindow.h"
#include "dialogs/connectiondialog.h"
#include "dialogs/commanddialog.h"
#include "dialogs/scriptdialog.h"
#include "widgets/collapsiblesection.h"
#include "core/logger.h"

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
#include <QApplication>
#include <QClipboard>
#include <QPixmap>
#include <QDialog>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QtConcurrent>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonParseError>

// ──────────────────────────────────────────────
//  Construction
// ──────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_activeConnectionId(-1)
    , m_titleLabel(nullptr)
    , m_toastLabel(nullptr)
    , m_toastTimer(nullptr)
{
    qRegisterMetaType<MqttConnectionConfig>("MqttConnectionConfig");

    setWindowTitle("MQTT 助手");
    setMinimumSize(960, 640);
    resize(1200, 780);
    setupMenuBar();
    setupUi();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }


    // 初始化数据库（关键修改）
    if (!initializeDatabase()) {
        // 如果用户取消或失败，显示错误并退出
        QMessageBox::critical(this, "错误",
                              "无法初始化数据库，程序将退出。\n"
                              "请确保有足够的权限或在下次启动时选择有效路径。");
        QTimer::singleShot(0, this, &QMainWindow::close);
        return;
    }
    loadAllData();
}

bool MainWindow::initializeDatabase()
{
    Logger::info("DB", "初始化数据库...");

    // 1. 首先检查是否有上次保存的路径
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    QString lastPath = settings.value("databasePath").toString();

    // 2. 如果上次路径存在且文件存在，直接使用
    if (!lastPath.isEmpty() && QFile::exists(lastPath)) {
        Logger::info("DB", "使用上次的数据库路径：" + lastPath);
        if (m_db.open(lastPath)) {
            qDebug() << "使用上次的数据库路径：" << lastPath;
            return true;
        } else {
            Logger::error("DB", "上次路径无法打开: " + m_db.lastError());
            // 上次路径存在但无法打开（可能已损坏）
            QMessageBox::warning(this, "警告",
                                 "上次使用的数据库文件存在但无法打开，可能已损坏。\n"
                                 "请选择新的存储位置或修复文件。");
        }
    }

    // 3. 如果上次路径不存在或文件不存在，引导用户选择
    return promptForDatabasePath();
}

bool MainWindow::promptForDatabasePath()
{
    while (true) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("选择数据库存储位置");
        msgBox.setText("未找到有效的数据库文件。");
        msgBox.setInformativeText("请选择数据存储方式：");

        QPushButton *useDefaultBtn = msgBox.addButton("使用默认位置", QMessageBox::AcceptRole);
        QPushButton *choosePathBtn = msgBox.addButton("选择存储位置", QMessageBox::ActionRole);
        QPushButton *exitBtn = msgBox.addButton("退出程序", QMessageBox::RejectRole);

        msgBox.setDefaultButton(useDefaultBtn);
        msgBox.exec();

        QAbstractButton *clicked = msgBox.clickedButton();
        QString selectedPath;

        if (clicked == useDefaultBtn) {
            // 使用 exe 所在目录作为默认位置
            selectedPath = QCoreApplication::applicationDirPath() + "/mqtt_assistant.db";
            qDebug() << "选择默认路径：" << selectedPath;
        }
        else if (clicked == choosePathBtn) {
            // 让用户选择路径
            selectedPath = QFileDialog::getSaveFileName(
                this,
                "选择或创建数据库文件",
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/mqtt_assistant.db",
                "SQLite Database (*.db);;All Files (*)"
                );

            if (selectedPath.isEmpty()) {
                // 用户取消了文件选择对话框，继续循环
                continue;
            }

            // 确保文件扩展名是 .db
            if (!selectedPath.endsWith(".db", Qt::CaseInsensitive)) {
                selectedPath += ".db";
            }
        }
        else if (clicked == exitBtn) {
            // 用户选择退出
            qDebug() << "用户选择退出程序";
            return false;
        }
        else {
            // 用户关闭了对话框（如点击右上角X）
            qDebug() << "用户关闭对话框";
            return false;
        }

        // 确保目录存在
        QFileInfo fileInfo(selectedPath);
        QDir dir(fileInfo.path());
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, "错误", "无法创建目录：" + dir.path());
                continue;
            }
        }

        // 尝试打开/创建数据库
        if (m_db.open(selectedPath)) {
            // 保存成功路径到设置
            saveDatabasePathToSettings(selectedPath);

            // 使用 QMessageBox 替代 showToast
            QMessageBox::information(this, "成功",
                                     "数据库已创建/打开：" + QFileInfo(selectedPath).fileName());

            return true;
        } else {
            // 打开失败，提示用户
            QString errorMsg = QString("无法在指定位置创建/打开数据库。\n"
                                       "请检查路径是否可写或选择其他位置。\n\n"
                                       "错误信息：%1")
                                   .arg(m_db.lastError());
            QMessageBox::warning(this, "错误", errorMsg);
            // 继续循环，让用户重新选择
        }
    }

    return false;
}

MainWindow::~MainWindow()
{
    // Collect thread pointers before stopClientThread() removes them from the map
    QList<QThread*> threads = m_clientThreads.values();
    for (int id : m_clients.keys())
        stopClientThread(id);
    // Wait for every worker thread to finish so we don't destroy a running thread
    for (QThread *t : threads)
        t->wait();
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

    // Designer credit on the right side of the status bar (requirement 4)
    QLabel *designerLabel = new QLabel("Designed by LJJ&YYJ", this);
    designerLabel->setStyleSheet("color: #999999; font-size: 11px; padding-right: 4px;");
    statusBar()->addPermanentWidget(designerLabel);

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
    m_titleLabel = new QLabel(sidebar);
    m_titleLabel->setObjectName("labelAppTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setFixedHeight(48);

    updateSidebarTitle();
    layout->addWidget(m_titleLabel);

    // Separator
    QFrame *sep = new QFrame(sidebar);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    layout->addWidget(sep);

    // ── Panels (as child widgets of CollapsibleSection) ──────────────
    m_connectionPanel = new ConnectionPanel();
    m_subscriptionPanel = new SubscriptionPanel();
    m_commandPanel = new CommandPanel();

    m_scriptList = new QListWidget();

    m_scriptList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_scriptList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ── Add-buttons for each section header ──────────────────────────
    auto makeAddBtn = [&](const QString &objName, const QString &tip) -> QPushButton * {
        QPushButton *btn = new QPushButton("+");
        btn->setObjectName(objName);
        btn->setFixedSize(24, 24);
        btn->setToolTip(tip);
        return btn;
    };
    QPushButton *addConnBtn   = makeAddBtn("btnAddConnection",   "新建连接");
    QPushButton *addSubBtn    = makeAddBtn("btnAddSubscription", "新增订阅");
    QPushButton *addCmdBtn    = makeAddBtn("btnAddCommand",      "新建命令");
    QPushButton *addScriptBtn = makeAddBtn("btnAddScript",       "新建脚本");

    // ── Collapsible sections ──────────────────────────────────────────
    auto *connSection   = new CollapsibleSection("连接管理",   m_connectionPanel);
    auto *subSection    = new CollapsibleSection("订阅管理",   m_subscriptionPanel);
    auto *cmdSection    = new CollapsibleSection("命令",       m_commandPanel);
    auto *scriptSection = new CollapsibleSection("脚本",       m_scriptList);

    connSection->addHeaderWidget(addConnBtn);
    subSection->addHeaderWidget(addSubBtn);
    cmdSection->addHeaderWidget(addCmdBtn);
    scriptSection->addHeaderWidget(addScriptBtn);

    // ── Vertical splitter for resizable sections ──────────────────────
    QSplitter *sectionSplitter = new QSplitter(Qt::Vertical, sidebar);
    sectionSplitter->setChildrenCollapsible(false);
    sectionSplitter->addWidget(connSection);
    sectionSplitter->addWidget(subSection);
    sectionSplitter->addWidget(cmdSection);
    sectionSplitter->addWidget(scriptSection);
    sectionSplitter->setStretchFactor(0, 2);
    sectionSplitter->setStretchFactor(1, 1);
    sectionSplitter->setStretchFactor(2, 1);
    sectionSplitter->setStretchFactor(3, 2);

    layout->addWidget(sectionSplitter, 1);

    // ── Wire up signals ───────────────────────────────────────────────
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
    connect(m_subscriptionPanel, &SubscriptionPanel::copyTopicRequested,
            [this](const QString &topic) {
                showToast("主题已复制到剪贴板: " + topic);
            });

    connect(m_commandPanel, &CommandPanel::editRequested,
            this, &MainWindow::onEditCommand);
    connect(m_commandPanel, &CommandPanel::deleteRequested,
            this, &MainWindow::onDeleteCommand);
    connect(m_commandPanel, &CommandPanel::addRequested,
            this, &MainWindow::onAddCommand);
    connect(m_commandPanel, &CommandPanel::commandSent,
            this, [this](const QString &topic, const QString &payload) {
                if (m_activeConnectionId >= 0) {
                    saveAndDisplayMessage(topic, payload, true, m_activeConnectionId);
                }
            });

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
    connect(m_chatWidget, &ChatWidget::clearHistoryRequested,
            this, &MainWindow::onClearHistoryRequested);

    connect(m_chatWidget, &ChatWidget::displayClearedRequested,
            this, [this](int connectionId) {
                if (connectionId >= 0)
                    m_chatClearedAt[connectionId] = QDateTime::currentDateTime();
            });

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
    m_monitorTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    m_monitorTable->setColumnWidth(0, 150);
    m_monitorTable->setColumnWidth(1, 75);
    m_monitorTable->setColumnWidth(2, 200);

    // 禁用文本换行
    m_monitorTable->setWordWrap(false);

    // 设置固定行高
    m_monitorTable->verticalHeader()->setDefaultSectionSize(25);
    m_monitorTable->verticalHeader()->setVisible(false);  // 隐藏行号

    // 设置选择模式
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitorTable->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置交替行颜色
    m_monitorTable->setAlternatingRowColors(true);
    m_monitorTable->verticalHeader()->setVisible(false);
    monitorLayout->addWidget(m_monitorTable);

    connect(m_monitorTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onMonitorRowDoubleClicked);

    m_tabWidget->addTab(monitorWidget, "监控");

    QPushButton *clearBtn = new QPushButton("清除", m_tabWidget);
    clearBtn->setObjectName("btnClearChat");  // 这个对象名很重要，匹配QSS
    clearBtn->setToolTip("清除聊天记录");

    m_tabWidget->setCornerWidget(clearBtn, Qt::TopRightCorner);
    connect(clearBtn, &QPushButton::clicked, m_chatWidget, &ChatWidget::onClearClicked);

    layout->addWidget(m_tabWidget);

    // Req 1: when switching back to the chat tab, scroll to the latest message
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int /*index*/) {
        if (m_tabWidget->currentWidget() == m_chatWidget)
            QTimer::singleShot(0, m_chatWidget, &ChatWidget::scrollToBottom);
    });
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
    qDebug() << "loadAllData: 开始加载连接";
    // Connections
    QList<MqttConnectionConfig> conns = m_db.loadConnections();
    qDebug() << "loadAllData: 加载到" << conns.size() << "个连接";
    for (const MqttConnectionConfig &c : conns) {
        m_connections[c.id] = c;
        m_connectionPanel->addConnection(c, false);
    }

    qDebug() << "loadAllData: 开始加载命令";
    // Commands
    QList<CommandConfig> cmds = m_db.loadCommands();
    qDebug() << "loadAllData: 加载到" << cmds.size() << "个命令";
    for (const CommandConfig &cmd : cmds)
        m_commands[cmd.id] = cmd;

    qDebug() << "loadAllData: 开始加载脚本";
    // Scripts
    QList<ScriptConfig> scripts = m_db.loadScripts();
    qDebug() << "loadAllData: 加载到" << scripts.size() << "个脚本";
    for (const ScriptConfig &s : scripts) {
        m_scripts[s.id] = s;
    }
    m_scriptEngine.setScripts(scripts);

    qDebug() << "loadAllData: 刷新脚本列表";
}

void MainWindow::refreshCommandPanel(int connectionId)
{
    m_commandPanel->clearCommands();
    for (const CommandConfig &cmd : m_commands.values()) {
        if (cmd.connectionId == connectionId || cmd.connectionId == -1)
            m_commandPanel->addCommand(cmd);
    }
}

void MainWindow::refreshScriptList(int connectionId)
{
    m_scriptList->blockSignals(true);
    m_scriptList->clear();
    for (const ScriptConfig &s : m_scripts.values()) {
        if (s.connectionId != connectionId && s.connectionId != -1)
            continue;
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
    for (const SubscriptionConfig &s : subs) {
        QMetaObject::invokeMethod(client, "subscribe", Qt::QueuedConnection,
                                  Q_ARG(QString, s.topic), Q_ARG(int, s.qos));
    }
}

void MainWindow::stopClientThread(int connectionId)
{
    if (m_clients.contains(connectionId)) {
        MqttClient *client = m_clients.take(connectionId);
        QMetaObject::invokeMethod(client, "disconnectFromHost", Qt::QueuedConnection);
        if (m_clientThreads.contains(connectionId)) {
            QThread *thread = m_clientThreads.take(connectionId);
            // Quit the event loop; client will be deleted via the
            // QThread::finished -> client->deleteLater() connection.
            thread->quit();
            // Do not block with thread->wait() to keep the UI responsive.
            // The thread and client will clean up asynchronously.
        }
    }
    m_unreadCounts.remove(connectionId);
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
        stopClientThread(connectionId);
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

    // Enforce maximum of 5 simultaneous connections
    int activeCount = 0;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->isConnected()) ++activeCount;
    }
    if (activeCount >= 5 && !m_clients.contains(connectionId)) {
        showToast("最多同时连接 5 个平台");
        return;
    }

    if (!m_clients.contains(connectionId)) {

        // Create client with no parent so it can be moved to a thread
        MqttClient *client = new MqttClient();
        QThread *thread = new QThread(this);

        // Move client to its own thread for UI-smooth operation
        client->moveToThread(thread);

        // Clean up client when thread finishes
        connect(thread, &QThread::finished, client, &QObject::deleteLater);
        connect(thread, &QThread::started, client, &MqttClient::init);
        thread->start();

        m_clients[connectionId]      = client;
        m_clientThreads[connectionId] = thread;
        m_unreadCounts[connectionId]  = 0;

        connect(client, &MqttClient::connected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, true);
            if (m_activeConnectionId == connectionId) {
                const QString &name = m_connections.value(connectionId).name;
                setWindowTitle("MQTT 助手 - " + name);
                m_statusLabel->setText("已连接：" + name);
                showToast("已连接到 " + name);
            }
            subscribeAllForConnection(connectionId);
        }, Qt::QueuedConnection);
        connect(client, &MqttClient::disconnected, this, [this, connectionId]() {
            m_connectionPanel->setConnected(connectionId, false);
            if (m_activeConnectionId == connectionId) {
                setWindowTitle("MQTT 助手");
                m_statusLabel->setText("已断开连接");
                showToast("已断开连接");
            }
        }, Qt::QueuedConnection);
        connect(client, &MqttClient::messageReceived, this,
                [this, connectionId](const QString &topic, const QString &payload, bool retained) {
                    if (m_activeConnectionId == connectionId) {
                        saveAndDisplayMessage(topic, payload, false, connectionId, retained);
                    } else {
                        // Save to DB and increment unread badge for inactive connection
                        if (!retained) {
                            MessageRecord msg;
                            msg.connectionId = connectionId;
                            msg.topic        = topic;
                            msg.payload      = payload;
                            msg.outgoing     = false;
                            msg.retained     = false;
                            msg.timestamp    = QDateTime::currentDateTime();
                            m_db.saveMessage(msg);

                            int count = m_unreadCounts.value(connectionId, 0) + 1;
                            m_unreadCounts[connectionId] = count;
                            m_connectionPanel->setUnreadCount(connectionId, count);
                        }
                    }
                }, Qt::QueuedConnection);
        connect(client, &MqttClient::errorOccurred, this,
                [this, connectionId](const QString &msg) {
                    m_connectionPanel->setLoading(connectionId, false);
                    showToast("错误：" + msg, 4000);
                }, Qt::QueuedConnection);
    }

    m_connectionPanel->setLoading(connectionId, true);

    MqttClient *client = m_clients[connectionId];
    QMetaObject::invokeMethod(client, "connectToHost", Qt::QueuedConnection,
                              Q_ARG(MqttConnectionConfig, config));

    m_activeConnectionId = connectionId;

    // 先断开所有可能的旧连接，避免重复
    disconnect(&m_scriptEngine, &ScriptEngine::messagePublished, this, nullptr);

    // 再建立新的连接
    // 注意：使用 m_activeConnectionId 运行时求值（而非闭包捕获 connectionId），
    // 避免在快速切换连接时，旧的 connectionId 被残留在闭包中导致消息丢失或保存到错误连接。
    connect(&m_scriptEngine, &ScriptEngine::messagePublished,
            this, [this](const QString &topic, const QString &payload) {
                int connId = m_activeConnectionId;
                if (connId < 0) {
                    Logger::warning("Script", "脚本触发发布，但无活动连接，消息已丢弃");
                    return;
                }
                Logger::debug("Script", QString("脚本触发发布 -> 连接[%1] topic=%2 payload=%3")
                                  .arg(connId).arg(topic, payload));
                saveAndDisplayMessage(topic, payload, true, connId);
            });

    QList<ScriptConfig> connScripts;
    for (const ScriptConfig &s : m_scripts.values()) {
        if (s.connectionId == connectionId || s.connectionId == -1)
            connScripts.append(s);
    }
    m_scriptEngine.setClient(client);
    m_scriptEngine.setScripts(connScripts);

    m_commandPanel->setClient(client);
    m_chatWidget->setClient(client);

    m_unreadCounts[connectionId] = 0;
    m_connectionPanel->clearUnreadCount(connectionId);

    m_chatClearedAt.remove(connectionId);

    // Refresh per-connection panels
    refreshCommandPanel(connectionId);
    refreshScriptList(connectionId);

    // Load message history
    loadMessagesAsync(connectionId);
}

void MainWindow::onDisconnectRequested(int connectionId)
{
    if (!m_clients.contains(connectionId)) return;
    MqttClient *client = m_clients[connectionId];
    QMetaObject::invokeMethod(client, "disconnectFromHost", Qt::QueuedConnection);
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

            // Update script engine for this connection
            QList<ScriptConfig> connScripts;
            for (const ScriptConfig &s : m_scripts.values()) {
                if (s.connectionId == connectionId || s.connectionId == -1)
                    connScripts.append(s);
            }
            m_scriptEngine.setClient(m_clients[connectionId]);
            m_scriptEngine.setScripts(connScripts);
        } else {
            setWindowTitle("MQTT 助手");
            m_statusLabel->setText("未连接");
            m_commandPanel->setClient(nullptr);
            m_chatWidget->setClient(nullptr);
            m_scriptEngine.setClient(nullptr);
            // 断开脚本引擎的信号连接
            disconnect(&m_scriptEngine, &ScriptEngine::messagePublished, this, nullptr);
        }

        // Reset unread badge for this connection
        m_unreadCounts[connectionId] = 0;
        m_connectionPanel->clearUnreadCount(connectionId);

        // Refresh per-connection panels
        refreshCommandPanel(connectionId);
        refreshScriptList(connectionId);

        // Load subscriptions for this connection
        QList<SubscriptionConfig> subs = m_db.loadSubscriptions(connectionId);
        m_subscriptionPanel->loadSubscriptions(subs);

        // Load message history
        loadMessagesAsync(connectionId);
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
        MqttClient *client = m_clients[m_activeConnectionId];
        QMetaObject::invokeMethod(client, "subscribe", Qt::QueuedConnection,
                                  Q_ARG(QString, topic), Q_ARG(int, 0));
    }
    showToast("已订阅：" + topic);
}

void MainWindow::onUnsubscribeRequested(const QString &topic, int id)
{
    m_db.deleteSubscription(id);
    m_subscriptionPanel->removeSubscriptionById(id);

    if (m_clients.contains(m_activeConnectionId) &&
        m_clients[m_activeConnectionId]->isConnected()) {
        MqttClient *client = m_clients[m_activeConnectionId];
        QMetaObject::invokeMethod(client, "unsubscribe", Qt::QueuedConnection,
                                  Q_ARG(QString, topic));
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
    refreshCommandPanel(m_activeConnectionId);
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
    refreshCommandPanel(m_activeConnectionId);
    showToast("命令已更新");
}

void MainWindow::onDeleteCommand(int commandId)
{
    int ret = QMessageBox::question(this, "删除命令",
        "确定要删除此命令吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_db.deleteCommand(commandId);
    m_commands.remove(commandId);
    refreshCommandPanel(m_activeConnectionId);
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
    refreshScriptList(m_activeConnectionId);
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
    refreshScriptList(m_activeConnectionId);
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
    refreshScriptList(m_activeConnectionId);
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
    QMetaObject::invokeMethod(client, "publish", Qt::QueuedConnection,
                              Q_ARG(QString, topic), Q_ARG(QString, payload),
                              Q_ARG(int, 0), Q_ARG(bool, false));
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
    QMetaObject::invokeMethod(client, "subscribe", Qt::QueuedConnection,
                              Q_ARG(QString, topic), Q_ARG(int, 0));

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
                                       bool outgoing, int connectionId, bool retained)
{
    MessageRecord msg;
    msg.connectionId = connectionId;
    msg.topic        = topic;
    msg.payload      = payload;
    msg.outgoing     = outgoing;
    msg.retained     = retained;
    msg.timestamp    = QDateTime::currentDateTime();

    // 检测数据类型
    if (payload.startsWith("HEX: ")) {
        msg.dataType = Hex;
    } else {
        // 尝试识别 JSON（用于后续显示优化）
        QJsonParseError err;
        QJsonDocument::fromJson(payload.toUtf8(), &err);
        msg.dataType = (err.error == QJsonParseError::NoError) ? Json : Text;
    }

    // 日志记录关键操作
    Logger::debug("Chat", QString("[%1] %2 topic=%3 size=%4B dataType=%5")
                              .arg(connectionId)
                              .arg(outgoing ? "OUT" : "IN ")
                              .arg(topic)
                              .arg(payload.size())
                              .arg(msg.dataType));

    // 保存到数据库（retained 消息不保存）
    if (!retained) {
        int id = m_db.saveMessage(msg);
        msg.id = id;
    }

    // 显示在界面上
    m_chatWidget->addMessage(msg);
    addMessageToMonitor(msg);
}

void MainWindow::addMessageToMonitor(const MessageRecord &msg)
{
    // 暂时禁用排序
    bool sortingEnabled = m_monitorTable->isSortingEnabled();
    m_monitorTable->setSortingEnabled(false);

    int row = m_monitorTable->rowCount();
    m_monitorTable->insertRow(row);

    // 时间项
    QString timeStr = msg.timestamp.isValid()
                          ? msg.timestamp.toString("yyyy-MM-dd hh:mm:ss")
                          : QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QTableWidgetItem *timeItem = new QTableWidgetItem(timeStr);
    timeItem->setTextAlignment(Qt::AlignCenter);
    timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsEditable);
    m_monitorTable->setItem(row, 0, timeItem);

    // 方向项
    QString dirText = msg.outgoing ? "↑ 发送" : "↓ 接收";
    QTableWidgetItem *dirItem = new QTableWidgetItem(dirText);
    dirItem->setTextAlignment(Qt::AlignCenter);
    dirItem->setFlags(dirItem->flags() & ~Qt::ItemIsEditable);

    QColor dirColor = msg.outgoing ? QColor("#ea5413") : QColor("#1e9e50");
    dirItem->setForeground(dirColor);
    m_monitorTable->setItem(row, 1, dirItem);

    // 主题项
    QString topicStr = msg.topic.trimmed();
    if (topicStr.isEmpty()) topicStr = "(空主题)";

    QTableWidgetItem *topicItem = new QTableWidgetItem(topicStr);
    topicItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    topicItem->setFlags(topicItem->flags() & ~Qt::ItemIsEditable);
    topicItem->setToolTip(topicStr);
    m_monitorTable->setItem(row, 2, topicItem);

    // 内容项 - 专门处理 HEX 格式
    QString payloadStr = msg.payload;

    // 处理空内容
    if (payloadStr.isEmpty()) {
        payloadStr = "(空消息)";
    }

    // 如果是 HEX 格式，保持原样，只做必要的清理
    if (payloadStr.startsWith("HEX: ")) {
        // 保持 HEX 格式不变，只替换换行符为空格，使其单行显示
        payloadStr.replace('\n', ' ');
        payloadStr.replace('\r', ' ');

        // 压缩多个连续空格，但保留 HEX 数据中的单个空格
        QStringList parts = payloadStr.split(' ', Qt::SkipEmptyParts);
        if (parts.size() > 0 && parts[0] == "HEX:") {
            // 重新组合，确保 "HEX:" 后面有空格
            QString hexData = parts.mid(1).join(' ');
            payloadStr = "HEX: " + hexData;
        }
    } else {
        // 非 HEX 格式，进行一般清理
        QString cleanPayload;
        for (QChar c : payloadStr) {
            if (c.isPrint() || c == ' ') {
                cleanPayload.append(c);
            } else if (c == '\n' || c == '\r' || c == '\t') {
                cleanPayload.append(' ');
            }
        }
        payloadStr = cleanPayload.simplified();
    }

    // 如果清理后为空，设置默认值
    if (payloadStr.isEmpty()) {
        payloadStr = "(空消息)";
    }

    // 限制显示长度
    QString displayPayload = payloadStr;
    QString fullPayload = payloadStr;

    if (displayPayload.length() > 200) {
        displayPayload = displayPayload.left(200) + "...";
    }

    QTableWidgetItem *payloadItem = new QTableWidgetItem(displayPayload);
    payloadItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    payloadItem->setFlags(payloadItem->flags() & ~Qt::ItemIsEditable);
    payloadItem->setToolTip(fullPayload);
    m_monitorTable->setItem(row, 3, payloadItem);

    // 设置行高
    m_monitorTable->setRowHeight(row, 25);

    // 恢复排序设置
    m_monitorTable->setSortingEnabled(sortingEnabled);

    // 滚动到底部
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

void MainWindow::saveDatabasePathToSettings(const QString &path)
{
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    settings.setValue("databasePath", path);
}

QString MainWindow::loadDatabasePathFromSettings()
{
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    return settings.value("databasePath").toString();
}

// ──────────────────────────────────────────────
//  Clear History Slot (Requirement 1)
// ──────────────────────────────────────────────

void MainWindow::onClearHistoryRequested(int connectionId)
{
    if (connectionId >= 0) {
        m_db.deleteMessages(connectionId);
        // DB cleared — no need to filter future loads for this connection
        m_chatClearedAt.remove(connectionId);
    }
    // Also clear the monitor table so it reflects the cleared state
    m_monitorTable->setRowCount(0);
    showToast("聊天记录已清除");
}

void MainWindow::loadMessagesAsync(int connectionId)
{
    m_chatWidget->setConnectionId(connectionId);
    m_chatWidget->clearMessages();
    m_monitorTable->setRowCount(0);

    const QDateTime clearTime = m_chatClearedAt.value(connectionId, QDateTime());
    const QString dbPath = m_db.databasePath();

    auto *watcher = new QFutureWatcher<QList<MessageRecord>>(this);
    connect(watcher, &QFutureWatcher<QList<MessageRecord>>::finished, this,
            [this, connectionId, watcher, clearTime]() {
                watcher->deleteLater();
                if (m_activeConnectionId != connectionId) {
                    return;
                }

                QList<MessageRecord> history = watcher->result();

                if (clearTime.isValid()) {
                    auto it = std::remove_if(history.begin(), history.end(),
                                             [clearTime](const MessageRecord &m) {
                                                 return m.timestamp < clearTime;
                                             });
                    history.erase(it, history.end());
                }

                m_chatWidget->loadMessages(history);

                // 批量添加监控消息
                m_monitorTable->setSortingEnabled(false);
                m_monitorTable->setUpdatesEnabled(false);

                for (const MessageRecord &msg : history) {
                    addMessageToMonitor(msg);
                }

                m_monitorTable->setUpdatesEnabled(true);
                m_monitorTable->setSortingEnabled(true);

                if (m_monitorTable->rowCount() > 0) {
                    m_monitorTable->scrollToBottom();
                }
            });

    QFuture<QList<MessageRecord>> future = QtConcurrent::run(
        [dbPath, connectionId]() -> QList<MessageRecord> {
            const QString connName = QStringLiteral("mqtt_load_") +
                                     QUuid::createUuid().toString(QUuid::WithoutBraces);
            QList<MessageRecord> result;

            {
                QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
                db.setDatabaseName(dbPath);

                if (db.open()) {
                    // 修改查询，包含 data_type 字段
                    QSqlQuery q(db);
                    q.prepare(
                        "SELECT id, connection_id, topic, payload, outgoing, timestamp, data_type "
                        "FROM messages WHERE connection_id=:connid "
                        "ORDER BY id DESC LIMIT 100"
                        );
                    q.bindValue(":connid", connectionId);

                    if (q.exec()) {
                        while (q.next()) {
                            MessageRecord m;
                            m.id           = q.value(0).toInt();
                            m.connectionId = q.value(1).toInt();
                            m.topic        = q.value(2).toString();

                            // 根据 data_type 处理 payload
                            int dataType = q.value(6).toInt();
                            QString payload = q.value(3).toString();

                            if (dataType == 1 || payload.startsWith("HEX: ")) {
                                // HEX 格式，保持原样
                                m.payload = payload;
                                m.dataType = Hex;
                            } else {
                                // 普通文本
                                m.payload = payload;
                                m.dataType = Text;
                            }

                            m.outgoing     = q.value(4).toBool();
                            m.retained     = false;
                            m.timestamp    = QDateTime::fromString(
                                q.value(5).toString(), Qt::ISODate);

                            result.prepend(m);
                        }
                    }
                    db.close();
                }
            }

            QSqlDatabase::removeDatabase(connName);
            return result;
        });

    watcher->setFuture(future);
}

// ──────────────────────────────────────────────
//  Monitor Row Double-Click (Requirement 8)
// ──────────────────────────────────────────────

void MainWindow::onMonitorRowDoubleClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_monitorTable->rowCount()) {
        return;
    }

    QTableWidgetItem *timeItem = m_monitorTable->item(row, 0);
    QTableWidgetItem *dirItem = m_monitorTable->item(row, 1);
    QTableWidgetItem *topicItem = m_monitorTable->item(row, 2);
    QTableWidgetItem *payloadItem = m_monitorTable->item(row, 3);

    if (!timeItem || !dirItem || !topicItem || !payloadItem) {
        QMessageBox::warning(this, "错误", "无法获取消息详情");
        return;
    }

    QString time    = timeItem->text();
    QString dir     = dirItem->text();
    QString topic   = topicItem->text();
    QString payload = payloadItem->toolTip().isEmpty() ? payloadItem->text() : payloadItem->toolTip();

    QDialog dlg(this);
    dlg.setWindowTitle("消息详情 - " + topic);
    dlg.setMinimumSize(600, 400);
    dlg.resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    // 信息显示区域
    QFrame *infoFrame = new QFrame(&dlg);
    infoFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoFrame);

    QLabel *infoLabel = new QLabel(infoFrame);
    infoLabel->setTextFormat(Qt::RichText);
    infoLabel->setText(
        QString("<b>时间:</b> %1<br>"
                "<b>方向:</b> %2<br>"
                "<b>主题:</b> %3")
            .arg(time.toHtmlEscaped(),
                 dir.toHtmlEscaped(),
                 topic.toHtmlEscaped()));
    infoLabel->setWordWrap(true);
    infoLayout->addWidget(infoLabel);

    layout->addWidget(infoFrame);

    // 内容显示区域
    QLabel *contentLabel = new QLabel("消息内容:", &dlg);
    layout->addWidget(contentLabel);

    QTextEdit *contentEdit = new QTextEdit(&dlg);

    // 如果是 HEX 数据，格式化显示
    if (payload.startsWith("HEX: ")) {
        // 移除 "HEX: " 前缀
        QString hexData = payload.mid(5).trimmed();

        // 按空格分割字节
        QStringList bytes = hexData.split(' ', Qt::SkipEmptyParts);

        // 格式化为每行16字节
        QStringList lines;
        lines << "HEX:";
        for (int i = 0; i < bytes.size(); i += 16) {
            QString line = "    ";
            // 添加字节数据
            QStringList rowBytes = bytes.mid(i, 16);
            for (int j = 0; j < rowBytes.size(); ++j) {
                line += rowBytes[j];
                if (j < rowBytes.size() - 1) {
                    line += ' ';
                }
            }
            // 添加ASCII表示
            line += "    |";
            for (int j = 0; j < 16; ++j) {
                if (i + j < bytes.size()) {
                    bool ok;
                    int byteVal = bytes[i + j].toInt(&ok, 16);
                    if (ok && byteVal >= 32 && byteVal <= 126) {
                        line += QChar(byteVal);
                    } else {
                        line += '.';
                    }
                } else {
                    line += ' ';
                }
            }
            line += '|';
            lines << line;
        }

        contentEdit->setPlainText(lines.join('\n'));
    } else {
        contentEdit->setPlainText(payload);
    }

    contentEdit->setReadOnly(true);
    contentEdit->setFontFamily("Consolas, Monaco, Courier New, monospace");

    layout->addWidget(contentEdit);

    // 按钮区域
    QDialogButtonBox *bbox = new QDialogButtonBox(&dlg);

    bbox->addButton("关闭", QDialogButtonBox::RejectRole);
    QPushButton *copyBtn = bbox->addButton("复制原始数据", QDialogButtonBox::ActionRole);
    QPushButton *copyHexBtn = bbox->addButton("复制HEX格式", QDialogButtonBox::ActionRole);

    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(copyBtn, &QPushButton::clicked, [&dlg, payload]() {
        QApplication::clipboard()->setText(payload);
        dlg.setWindowTitle("消息详情 - 已复制原始数据");
        QTimer::singleShot(1000, &dlg, [&dlg]() {
            dlg.setWindowTitle("消息详情");
        });
    });

    connect(copyHexBtn, &QPushButton::clicked, [&dlg, payload]() {
        if (payload.startsWith("HEX: ")) {
            // 只复制HEX数据部分，不带格式
            QString hexData = payload.mid(5).trimmed();
            QApplication::clipboard()->setText(hexData);
        } else {
            QApplication::clipboard()->setText(payload);
        }
        dlg.setWindowTitle("消息详情 - 已复制HEX数据");
        QTimer::singleShot(1000, &dlg, [&dlg]() {
            dlg.setWindowTitle("消息详情");
        });
    });

    layout->addWidget(bbox);

    dlg.exec();
}

// ──────────────────────────────────────────────
//  Sidebar Title (Requirement 6)
// ──────────────────────────────────────────────

void MainWindow::updateSidebarTitle()
{
    if (!m_titleLabel) return;
    // ---- Modify the path below to change the title image ----
    const QString kTitleImagePath = QCoreApplication::applicationDirPath() + "/e-linter.png";
    // ---------------------------------------------------------
    QPixmap pm(kTitleImagePath);
    if (!pm.isNull()) {
        static const int kTitleW = 499 * 0.26;
        static const int kTitleH = 146 * 0.26;
        m_titleLabel->setPixmap(pm.scaled(kTitleW, kTitleH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_titleLabel->setPixmap(QPixmap());
        m_titleLabel->setText(QString());
    }
}
