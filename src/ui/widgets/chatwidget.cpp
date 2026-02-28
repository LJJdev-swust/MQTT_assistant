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
#include <QCoreApplication>

// Definition for the static integral member declared in the header.
// Required because although it's initialized in-class, some uses
// can require an out-of-line definition to satisfy the linker.
const int ChatWidget::kLoadBatchSize;
const int ChatWidget::kLazyPageSize;

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent), m_client(nullptr), m_connectionId(-1), m_pendingScrollCount(0),
    m_loadGeneration(0), m_oldestLoadedId(-1), m_allHistoryLoaded(false),
    m_loadingOlderMessages(false), m_sliderBeingDragged(false),
    m_loadingOverlay(nullptr), m_loadingSpinner(nullptr),
    m_spinnerAngle(0), m_spinnerTimer(nullptr)
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
    m_clearPayloadBtn = new QPushButton("清除", inputArea);
    m_clearPayloadBtn->setFixedWidth(70);
    m_clearPayloadBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_clearPayloadBtn->setToolTip("清除输入框内容");

    // 按钮竖向排列在右侧
    QVBoxLayout *btnCol = new QVBoxLayout();
    btnCol->setSpacing(4);
    btnCol->addWidget(m_sendBtn);
    btnCol->addStretch();
    btnCol->addWidget(m_clearPayloadBtn);
    btnCol->addStretch();

    payloadRow->addWidget(m_payloadEdit);
    payloadRow->addLayout(btnCol);

    inputLayout->addLayout(topicRow);
    inputLayout->addLayout(payloadRow);

    m_splitter->addWidget(inputArea);

    // Set initial split sizes: messages take ~70%, input ~30%
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({350, 130});

    mainLayout->addWidget(m_splitter, 1);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_subscribeBtn, &QPushButton::clicked, this, &ChatWidget::onSubscribeClicked);
    connect(m_clearPayloadBtn, &QPushButton::clicked, m_payloadEdit, &QTextEdit::clear);

    // ---- Loading overlay (parented to scrollArea viewport) ----
    m_loadingOverlay = new QWidget(m_scrollArea->viewport());
    m_loadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_loadingOverlay->setStyleSheet(
        "background: rgba(245,245,245,200); border-radius: 8px;");
    m_loadingOverlay->hide();

    // Spinner label — we'll draw a custom arc in a QLabel subclass via paintEvent trick:
    // Instead, use a QLabel with a Unicode spinner char and rotate it via QTimer.
    m_loadingSpinner = new QLabel(m_loadingOverlay);
    m_loadingSpinner->setAlignment(Qt::AlignCenter);
    m_loadingSpinner->setStyleSheet(
        "font-size: 14px; color: #555555; background: transparent;");

    QLabel *loadingText = new QLabel("加载中...", m_loadingOverlay);
    loadingText->setAlignment(Qt::AlignCenter);
    loadingText->setStyleSheet("font-size: 13px; color: #666666; background: transparent;");

    QVBoxLayout *overlayLayout = new QVBoxLayout(m_loadingOverlay);
    overlayLayout->setContentsMargins(16, 16, 16, 16);
    overlayLayout->setSpacing(8);
    overlayLayout->addStretch();
    overlayLayout->addWidget(m_loadingSpinner, 0, Qt::AlignCenter);
    overlayLayout->addWidget(loadingText, 0, Qt::AlignCenter);
    overlayLayout->addStretch();

    // Spinner animation: rotate unicode arc every 80ms
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(120);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
            {
                m_spinnerAngle = (m_spinnerAngle + 1) % 4;
                static const QStringList frames = {"◐","◓","◑","◒"};
                m_loadingSpinner->setText(
                    QString("<span style='font-size:28px;'>%1</span>").arg(frames[m_spinnerAngle])); });

    // Track drag state of the scroll bar (suppress scroll-jump while dragging)
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::sliderPressed, this, [this]()
            { m_sliderBeingDragged = true; });
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::sliderReleased, this, [this]()
            { m_sliderBeingDragged = false; });

    loadTopicHistory();
}

void ChatWidget::setClient(MqttClient *client)
{
    m_client = client;
}

void ChatWidget::addMessage(const MessageRecord &msg)
{
    m_connectionId = msg.connectionId;

    // If the container is still frozen from a previous clearMessages() call
    // (e.g. history load hasn't returned yet), unfreeze it so the live message
    // is visible immediately.
    if (!m_messagesContainer->updatesEnabled())
        m_messagesContainer->setUpdatesEnabled(true);

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bool wasAtBottom = (bar->maximum() == 0 || bar->value() >= bar->maximum() - 4);

    MessageBubbleItem *bubble = new MessageBubbleItem(msg, m_messagesContainer);
    // Insert before the trailing stretch
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);

    if (wasAtBottom)
    {
        QTimer::singleShot(50, this, &ChatWidget::scrollToBottom);
    }
    else
    {
        ++m_pendingScrollCount;
        updateScrollToBottomBtn();
    }
}

void ChatWidget::clearMessages()
{
    // Cancel any pending batch load by advancing the generation counter
    ++m_loadGeneration;
    m_loadQueue.clear();
    m_oldestLoadedId = -1;
    m_allHistoryLoaded = false;
    m_loadingOlderMessages = false;

    // Hide the loading overlay in case a previous load was in progress
    hideLoadingOverlay();

    // Freeze painting so the removal loop doesn't trigger intermediate repaints.
    // IMPORTANT: we do NOT call setUpdatesEnabled(true) here.
    // • loadMessages() will unfreeze after the overlay is shown, hiding the
    //   blank-container flicker and the deleteLater() ghost frame.
    // • standalone callers (onClearClicked) must call unfreezeMessages() themselves.
    m_messagesContainer->setUpdatesEnabled(false);

    // Remove all bubble items (all except the trailing stretch)
    while (m_messagesLayout->count() > 1)
    {
        QLayoutItem *item = m_messagesLayout->takeAt(0);
        if (item->widget())
        {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
    // Container stays frozen — caller is responsible for unfreezing.
}

// Re-enable updates on the message container (standalone clear / API compatibility)
void ChatWidget::unfreezeMessages()
{
    m_messagesContainer->setUpdatesEnabled(true);
}

void ChatWidget::loadMessages(const QList<MessageRecord> &messages)
{
    clearMessages(); // cancels old generation, removes old bubbles, freezes container

    if (messages.isEmpty())
    {
        unfreezeMessages(); // nothing to show, just unfreeze
        return;
    }

    m_connectionId = messages.first().connectionId;
    m_oldestLoadedId = messages.first().id;

    // Show spinner overlay BEFORE unfreezing the container.
    // The overlay paints on top of the frozen (blank) surface, so the user
    // never sees the old bubbles' deleteLater() ghost frame.
    showLoadingOverlay();

    // Queue all records for batch processing
    m_loadQueue = messages;
    int gen = m_loadGeneration;

    // Yield one tick: this lets deleteLater() objects be destroyed and the
    // overlay to fully paint, THEN unfreeze and start inserting batches.
    QTimer::singleShot(0, this, [this, gen]()
                       {
                           if (gen != m_loadGeneration) return;
                           m_messagesContainer->setUpdatesEnabled(true); // unfreeze: container is now empty & clean
                           processNextBatch(gen); });
}

// processNextBatch: Insert up to kLoadBatchSize bubbles per event-loop tick.
// Updates are disabled during each batch to avoid per-widget layout passes,
// then re-enabled so Qt flushes the batch as a single paint.
// Between batches we yield to the event loop so the UI stays responsive
// (user can click, resize, switch tabs, etc. while history is rendering).
void ChatWidget::processNextBatch(int generation)
{
    if (generation != m_loadGeneration || m_loadQueue.isEmpty())
    {
        // Stale or nothing left — hide overlay if this generation is still active
        if (generation == m_loadGeneration)
        {
            hideLoadingOverlay();
            QTimer::singleShot(0, this, [this, generation]()
                               {
                                   if (generation != m_loadGeneration) return;
                                   m_messagesContainer->updateGeometry();
                                   QTimer::singleShot(0, this, [this, generation]() {
                                       if (generation != m_loadGeneration) return;
                                       scrollToBottom();
                                   }); });
        }
        return;
    }

    int count = qMin(kLoadBatchSize, m_loadQueue.size());

    // Disable updates for the batch so Qt doesn't trigger a layout/paint per widget
    m_messagesContainer->setUpdatesEnabled(false);
    for (int i = 0; i < count; ++i)
    {
        MessageBubbleItem *bubble = new MessageBubbleItem(m_loadQueue.at(i), m_messagesContainer);
        m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    }
    m_messagesContainer->setUpdatesEnabled(true); // flush this batch as one paint

    // Remove processed items
    m_loadQueue.erase(m_loadQueue.begin(), m_loadQueue.begin() + count);

    if (!m_loadQueue.isEmpty())
    {
        // Yield to the event loop before the next batch so UI stays interactive
        QTimer::singleShot(0, this, [this, generation]()
                           { processNextBatch(generation); });
    }
    else
    {
        // All done — hide the overlay and scroll to bottom
        hideLoadingOverlay();
        QTimer::singleShot(0, this, [this, generation]()
                           {
                               if (generation != m_loadGeneration) return;
                               m_messagesContainer->updateGeometry();
                               QTimer::singleShot(0, this, [this, generation]() {
                                   if (generation != m_loadGeneration) return;
                                   scrollToBottom();
                               }); });
    }
}

// Prepend older messages at the top of the chat (lazy/infinite scroll).
// Keeps the scroll position stable so the user doesn't lose their place.
void ChatWidget::prependMessages(const QList<MessageRecord> &messages)
{
    if (messages.isEmpty())
    {
        m_allHistoryLoaded = true;
        m_loadingOlderMessages = false;
        return;
    }

    // Remember scroll position before inserting widgets
    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    int oldMax = bar->maximum();
    int oldValue = bar->value();

    // Insert with updates disabled to avoid intermediate repaints
    m_messagesContainer->setUpdatesEnabled(false);

    // Insert in reverse so that order is maintained (messages are oldest-first)
    for (int i = messages.size() - 1; i >= 0; --i)
    {
        MessageBubbleItem *bubble = new MessageBubbleItem(messages.at(i), m_messagesContainer);
        m_messagesLayout->insertWidget(0, bubble);
    }

    m_messagesContainer->setUpdatesEnabled(true);

    // Update oldest id
    m_oldestLoadedId = messages.first().id;

    // If the server returned fewer rows than a full page, we've reached the beginning
    if (messages.size() < kLazyPageSize)
        m_allHistoryLoaded = true;

    // Helper lambda that actually restores the scroll position.
    // Captured after layout settles (two ticks: one for layout, one for scrollbar max).
    auto doRestoreScroll = [this, oldMax, oldValue]()
    {
        m_messagesContainer->updateGeometry();
        QTimer::singleShot(0, this, [this, oldMax, oldValue]()
                           {
                               QScrollBar *b = m_scrollArea->verticalScrollBar();
                               int delta = b->maximum() - oldMax;
                               if (delta > 0)
                                   b->setValue(oldValue + delta);
                               // Mark loading done only after scroll is restored
                               m_loadingOlderMessages = false; });
    };

    if (m_sliderBeingDragged)
    {
        // User is currently dragging the scrollbar thumb.
        // Restoring position immediately would cause the thumb to jump.
        // Wait until the user releases, then restore.
        // Use a single-shot connection so it fires exactly once.
        QMetaObject::Connection *connPtr = new QMetaObject::Connection();
        *connPtr = connect(m_scrollArea->verticalScrollBar(), &QScrollBar::sliderReleased,
                           this, [this, doRestoreScroll, connPtr]()
                           {
                               disconnect(*connPtr);
                               delete connPtr;
                               QTimer::singleShot(0, this, doRestoreScroll); });
    }
    else
    {
        // No drag in progress — restore immediately after layout settles
        QTimer::singleShot(0, this, doRestoreScroll);
    }
}

void ChatWidget::onSendClicked()
{
    QString topic = m_topicCombo->currentText().trimmed();
    QString payload = m_payloadEdit->toPlainText();
    if (topic.isEmpty())
        return;

    // Validate: publish topic must not contain '#'
    if (topic.contains('#'))
    {
        QMessageBox::warning(this, "主题格式错误",
                             "发布主题不能包含通配符 '#'，请修正主题后重试。");
        return;
    }

    emit sendRequested(topic, payload);
    // 不再自动清除，由用户手动点击"清除"按钮

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
    unfreezeMessages(); // standalone clear — re-enable painting immediately
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
        m_scrollArea->verticalScrollBar()->maximum());
    m_pendingScrollCount = 0;
    m_scrollToBottomBtn->hide();
}

// ─────────────────────────────────────────────────────
//  Scroll-to-bottom button helpers
// ─────────────────────────────────────────────────────

void ChatWidget::onScrollValueChanged(int value)
{
    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    bool atBottom = (bar->maximum() == 0 || value >= bar->maximum() - 4);

    if (atBottom)
    {
        // Reached the bottom: clear badge and hide button
        m_pendingScrollCount = 0;
        m_scrollToBottomBtn->hide();
    }
    else
    {
        // Not at bottom: always show the button (with or without unread badge)
        updateScrollToBottomBtn();
    }

    // Reached the top: request older messages (lazy load)
    // Guard: don't fire again while a previous request is still in-flight
    if (value == 0 && !m_allHistoryLoaded && m_oldestLoadedId > 0 && !m_loadingOlderMessages)
    {
        m_loadingOlderMessages = true;
        emit requestMoreMessages(m_connectionId, m_oldestLoadedId);
    }
}

void ChatWidget::updateScrollToBottomBtn()
{
    if (m_pendingScrollCount > 0)
    {
        m_scrollToBottomBtn->setText(
            QString("↓ %1 条新消息").arg(m_pendingScrollCount));
    }
    else
    {
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

void ChatWidget::repositionLoadingOverlay()
{
    if (!m_loadingOverlay)
        return;
    QWidget *vp = m_scrollArea->viewport();
    // Center a 160x120 panel in the viewport
    const int w = 160, h = 120;
    m_loadingOverlay->setGeometry(
        (vp->width() - w) / 2,
        (vp->height() - h) / 2,
        w, h);
}

void ChatWidget::showLoadingOverlay()
{
    if (!m_loadingOverlay)
        return;
    repositionLoadingOverlay();
    m_spinnerAngle = 0;
    m_loadingSpinner->setText("<span style='font-size:28px;'>◐</span>");
    m_spinnerTimer->start();
    m_loadingOverlay->show();
    m_loadingOverlay->raise();
}

void ChatWidget::hideLoadingOverlay()
{
    if (!m_loadingOverlay)
        return;
    m_spinnerTimer->stop();
    m_loadingOverlay->hide();
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_scrollArea->viewport() && event->type() == QEvent::Resize)
    {
        if (m_scrollToBottomBtn->isVisible())
            repositionScrollToBottomBtn();
        if (m_loadingOverlay && m_loadingOverlay->isVisible())
            repositionLoadingOverlay();
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
    for (const QString &t : topics)
    {
        if (!t.isEmpty())
            m_topicCombo->addItem(t);
    }
}
