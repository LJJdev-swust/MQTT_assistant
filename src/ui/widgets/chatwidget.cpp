#include "chatwidget.h"
#include "messagebubbleitem.h"
#include "core/mqttclient.h"
#include <QScrollBar>
#include <QTimer>
#include <QLabel>
#include <QFrame>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Scroll area for messages ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_messagesContainer = new QWidget();
    m_messagesContainer->setObjectName("messagesContainer");
    m_messagesContainer->setStyleSheet("background-color: #1e1e2e;");

    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(4, 4, 4, 4);
    m_messagesLayout->setSpacing(4);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // ---- Input area ----
    QWidget *inputArea = new QWidget(this);
    inputArea->setObjectName("chatInputArea");
    inputArea->setFixedHeight(130);

    QVBoxLayout *inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(8, 6, 8, 6);
    inputLayout->setSpacing(4);

    // Topic row
    QHBoxLayout *topicRow = new QHBoxLayout();
    topicRow->setSpacing(6);
    QLabel *topicLabel = new QLabel("Topic:", inputArea);
    topicLabel->setFixedWidth(46);
    m_topicCombo = new QComboBox(inputArea);
    m_topicCombo->setEditable(true);
    m_topicCombo->setInsertPolicy(QComboBox::InsertAtTop);
    m_topicCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_topicCombo->lineEdit()->setPlaceholderText("Enter topic...");

    m_subscribeBtn = new QPushButton("Subscribe", inputArea);
    m_subscribeBtn->setFixedWidth(90);

    topicRow->addWidget(topicLabel);
    topicRow->addWidget(m_topicCombo);
    topicRow->addWidget(m_subscribeBtn);

    // Payload + Send row
    QHBoxLayout *payloadRow = new QHBoxLayout();
    payloadRow->setSpacing(6);
    m_payloadEdit = new QTextEdit(inputArea);
    m_payloadEdit->setPlaceholderText("Enter payload...");
    m_payloadEdit->setMaximumHeight(60);
    m_sendBtn = new QPushButton("Send", inputArea);
    m_sendBtn->setFixedSize(70, 60);

    payloadRow->addWidget(m_payloadEdit);
    payloadRow->addWidget(m_sendBtn);

    inputLayout->addLayout(topicRow);
    inputLayout->addLayout(payloadRow);

    mainLayout->addWidget(inputArea);

    connect(m_sendBtn,      &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_subscribeBtn, &QPushButton::clicked, this, &ChatWidget::onSubscribeClicked);
}

void ChatWidget::setClient(MqttClient *client)
{
    m_client = client;
}

void ChatWidget::addMessage(const MessageRecord &msg)
{
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
    emit sendRequested(topic, payload);
    m_payloadEdit->clear();

    // Remember topic in combo
    if (m_topicCombo->findText(topic) == -1)
        m_topicCombo->insertItem(0, topic);
}

void ChatWidget::onSubscribeClicked()
{
    QString topic = m_topicCombo->currentText().trimmed();
    if (!topic.isEmpty())
        emit subscribeRequested(topic);
}

void ChatWidget::scrollToBottom()
{
    m_scrollArea->verticalScrollBar()->setValue(
        m_scrollArea->verticalScrollBar()->maximum()
    );
}
