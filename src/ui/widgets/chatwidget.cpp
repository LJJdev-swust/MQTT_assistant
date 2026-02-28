#include "chatwidget.h"
#include "messagebubbleitem.h"
#include "core/mqttclient.h"
#include <QScrollBar>
#include <QTimer>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>
#include <QResizeEvent>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
    , m_connectionId(-1)
    , m_pendingScrollCount(0)
    , m_loadGeneration(0)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);


    // ---- Splitter: messages (top) / input (bottom) ----
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setHandleWidth(5);
    m_splitter->setChildrenCollapsible(false);

    // Scroll area for messages
    m_scrollArea = new QScrollArea(m_splitter);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_messagesContainer = new QWidget();
    m_messagesContainer->setObjectName("messagesContainer");
    m_messagesContainer->setStyleSheet("background-color: #f5f5f5;");

    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(4, 4, 4, 4);
    m_messagesLayout->setSpacing(4);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesContainer);
    m_splitter->addWidget(m_scrollArea);

    // Floating "scroll to bottom" button (parented to scroll area viewport)
    m_scrollToBottomBtn = new QPushButton("↓ 回到底部", m_scrollArea->viewport());
    m_scrollToBottomBtn->setObjectName("scrollToBottomBtn");
    m_scrollToBottomBtn->setCursor(Qt::PointingHandCursor);
    m_scrollToBottomBtn->adjustSize();
    m_scrollToBottomBtn->hide();
    m_scrollToBottomBtn->setFocusPolicy(Qt::NoFocus);

    // Install event filter on viewport to reposition button on resize
    m_scrollArea->viewport()->installEventFilter(this);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ChatWidget::onScrollValueChanged);
    connect(m_scrollToBottomBtn, &QPushButton::clicked, this, &ChatWidget::scrollToBottom);

    // Input area
    QWidget *inputArea = new QWidget(m_splitter);
    inputArea->setObjectName("chatInputArea");

    QVBoxLayout *inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(8, 6, 8, 6);
    inputLayout->setSpacing(4);

    // Topic row
    QHBoxLayout *topicRow = new QHBoxLayout();
    topicRow->setSpacing(6);
    QLabel *topicLabel = new QLabel("主题:", inputArea);
    topicLabel->setFixedWidth(46);
    m_topicCombo = new QComboBox(inputArea);
    m_topicCombo->setEditable(true);
    m_topicCombo->setInsertPolicy(QComboBox::NoInsert);
    m_topicCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_topicCombo->lineEdit()->setPlaceholderText("输入主题...");

    m_subscribeBtn = new QPushButton("订阅", inputArea);
    m_subscribeBtn->setFixedWidth(70);

    topicRow->addWidget(topicLabel);
    topicRow->addWidget(m_topicCombo);
    topicRow->addWidget(m_subscribeBtn);

    // Payload + Send row (inside input area so they don't scale with splitter)
    QHBoxLayout *payloadRow = new QHBoxLayout();
    payloadRow->setSpacing(6);
    m_payloadEdit = new QTextEdit(inputArea);
    m_payloadEdit->setPlaceholderText("输入消息内容...");
    m_sendBtn = new QPushButton("发送", inputArea);
    m_sendBtn->setFixedWidth(70);
    m_sendBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    payloadRow->addWidget(m_payloadEdit);
    payloadRow->addWidget(m_sendBtn);

    inputLayout->addLayout(topicRow);
    inputLayout->addLayout(payloadRow);

    m_splitter->addWidget(inputArea);

    // Set initial split sizes: messages take ~70%, input ~30%
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({350, 130});

    mainLayout->addWidget(m_splitter, 1);

    connect(m_sendBtn,      &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_subscribeBtn, &QPushButton::clicked, this, &ChatWidget::onSubscribeClicked);

    loadTopicHistory();
}

void ChatWidget::setClient(MqttClient *client)
{
    m_client = client;
}

void ChatWidget::addMessage(const MessageRecord &msg)
{
    m_connectionId = msg.connectionId;

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bool wasAtBottom = (bar->maximum() == 0 || bar->value() >= bar->maximum() - 4);

    MessageBubbleItem *bubble = new MessageBubbleItem(msg, m_messagesContainer);
    // Insert before the trailing stretch
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);

    if (wasAtBottom) {
        QTimer::singleShot(50, this, &ChatWidget::scrollToBottom);
    } else {
        ++m_pendingScrollCount;
        updateScrollToBottomBtn();
    }
}

void ChatWidget::clearMessages()
{
    // Cancel any pending batch load by advancing the generation counter
    ++m_loadGeneration;
    m_loadQueue.clear();

    // Remove all bubble items (all except the trailing stretch)
    while (m_messagesLayout->count() > 1) {
        QLayoutItem *item = m_messagesLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void ChatWidget::loadMessages(const QList<MessageRecord> &messages)
{
    clearMessages();
    if (messages.isEmpty())
        return;
    // All records in a batch share the same connection ID; set it once.
    m_connectionId = messages.first().connectionId;
    m_loadQueue = messages;
    int gen = m_loadGeneration; // captured so stale timers self-cancel
    QTimer::singleShot(0, this, [this, gen]() { processNextBatch(gen); });
}

// Insert up to kLoadBatchSize bubbles, then yield to the event loop.
// The generation parameter ensures that stale callbacks from a previous
// loadMessages() call (which may still be queued) do nothing.
void ChatWidget::processNextBatch(int generation)
{
    if (generation != m_loadGeneration || m_loadQueue.isEmpty())
        return;

    int count = qMin(kLoadBatchSize, m_loadQueue.size());
    m_messagesContainer->setUpdatesEnabled(false);
    for (int i = 0; i < count; ++i) {
        MessageBubbleItem *bubble = new MessageBubbleItem(m_loadQueue.at(i), m_messagesContainer);
        m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    }
    // Remove processed items in a single erase operation (avoids per-item shifting)
    m_loadQueue.erase(m_loadQueue.begin(), m_loadQueue.begin() + count);
    m_messagesContainer->setUpdatesEnabled(true);

    if (!m_loadQueue.isEmpty()) {
        // More items remain: yield to the event loop, then continue
        QTimer::singleShot(0, this, [this, generation]() { processNextBatch(generation); });
    } else {
        QTimer::singleShot(50, this, &ChatWidget::scrollToBottom);
    }
}

void ChatWidget::onSendClicked()
{
    QString topic   = m_topicCombo->currentText().trimmed();
    QString payload = m_payloadEdit->toPlainText();
    if (topic.isEmpty())
        return;

    // Validate: publish topic must not contain '#'
    if (topic.contains('#')) {
        QMessageBox::warning(this, "主题格式错误",
                             "发布主题不能包含通配符 '#'，请修正主题后重试。");
        return;
    }

    emit sendRequested(topic, payload);
    m_payloadEdit->clear();

    // Remember topic in combo (max kMaxTopicHistory)
    int existingIdx = m_topicCombo->findText(topic);
    if (existingIdx != -1)
        m_topicCombo->removeItem(existingIdx);
    m_topicCombo->insertItem(0, topic);
    while (m_topicCombo->count() > kMaxTopicHistory)
        m_topicCombo->removeItem(m_topicCombo->count() - 1);
    m_topicCombo->setCurrentIndex(0);

    saveTopicHistory();
}

void ChatWidget::onSubscribeClicked()
{
    QString topic = m_topicCombo->currentText().trimmed();
    if (!topic.isEmpty())
        emit subscribeRequested(topic);
}

void ChatWidget::onClearClicked()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("清除聊天记录");
    msgBox.setText("确定要清除聊天框中显示的内容吗？");
    msgBox.setIcon(QMessageBox::Question);

    QCheckBox *alsoDeleteCheck = new QCheckBox("同时清除已保存的聊天记录（不可恢复）", &msgBox);
    msgBox.setCheckBox(alsoDeleteCheck);

    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.button(QMessageBox::Ok)->setText("清除");
    msgBox.button(QMessageBox::Cancel)->setText("取消");

    if (msgBox.exec() != QMessageBox::Ok)
        return;

    clearMessages();
    m_pendingScrollCount = 0;
    m_scrollToBottomBtn->hide();

    // Always notify that the display was cleared so MainWindow can track it
    emit displayClearedRequested(m_connectionId);

    if (alsoDeleteCheck->isChecked() && m_connectionId >= 0)
        emit clearHistoryRequested(m_connectionId);
}

void ChatWidget::scrollToBottom()
{
    m_scrollArea->verticalScrollBar()->setValue(
        m_scrollArea->verticalScrollBar()->maximum()
        );
    m_pendingScrollCount = 0;
    m_scrollToBottomBtn->hide();
}

// ─────────────────────────────────────────────────────
//  Scroll-to-bottom button helpers
// ─────────────────────────────────────────────────────

void ChatWidget::onScrollValueChanged(int value)
{
    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    if (bar->maximum() == 0 || value >= bar->maximum() - 4) {
        // User has scrolled to (or is at) the bottom
        m_pendingScrollCount = 0;
        m_scrollToBottomBtn->hide();
    }
}

void ChatWidget::updateScrollToBottomBtn()
{
    if (m_pendingScrollCount > 0) {
        m_scrollToBottomBtn->setText(
            QString("↓ %1 条新消息").arg(m_pendingScrollCount));
    } else {
        m_scrollToBottomBtn->setText("↓ 回到底部");
    }
    m_scrollToBottomBtn->adjustSize();
    repositionScrollToBottomBtn();
    m_scrollToBottomBtn->show();
    m_scrollToBottomBtn->raise();
}

void ChatWidget::repositionScrollToBottomBtn()
{
    QWidget *vp = m_scrollArea->viewport();
    int x = (vp->width() - m_scrollToBottomBtn->width()) / 2;
    int y = vp->height() - m_scrollToBottomBtn->height() - 20;
    m_scrollToBottomBtn->move(x, y);
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        if (m_scrollToBottomBtn->isVisible())
            repositionScrollToBottomBtn();
    }
    return QWidget::eventFilter(obj, event);
}

void ChatWidget::saveTopicHistory()
{
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    QStringList topics;
    for (int i = 0; i < m_topicCombo->count(); ++i)
        topics.append(m_topicCombo->itemText(i));
    settings.setValue("chat/topicHistory", topics);
}

void ChatWidget::loadTopicHistory()
{
    QSettings settings("MQTTAssistant", "MQTT_assistant");
    QStringList topics = settings.value("chat/topicHistory").toStringList();
    for (const QString &t : topics) {
        if (!t.isEmpty())
            m_topicCombo->addItem(t);
    }
}
