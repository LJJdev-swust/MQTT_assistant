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

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
    , m_connectionId(-1)
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
    MessageBubbleItem *bubble = new MessageBubbleItem(msg, m_messagesContainer);
    // Insert before the trailing stretch
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    QTimer::singleShot(50, this, &ChatWidget::scrollToBottom);
}

void ChatWidget::clearMessages()
{
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
    for (const MessageRecord &msg : messages)
        addMessage(msg);
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

    if (alsoDeleteCheck->isChecked() && m_connectionId >= 0)
        emit clearHistoryRequested(m_connectionId);
}

void ChatWidget::scrollToBottom()
{
    m_scrollArea->verticalScrollBar()->setValue(
        m_scrollArea->verticalScrollBar()->maximum()
        );
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
