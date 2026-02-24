#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QList>
#include "core/models.h"

class MqttClient;

class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void setClient(MqttClient *client);
    void addMessage(const MessageRecord &msg);
    void clearMessages();
    void loadMessages(const QList<MessageRecord> &messages);

    QComboBox *topicCombo() const { return m_topicCombo; }

signals:
    void sendRequested(const QString &topic, const QString &payload);
    void subscribeRequested(const QString &topic);

private slots:
    void onSendClicked();
    void onSubscribeClicked();
    void scrollToBottom();

private:
    QScrollArea  *m_scrollArea;
    QWidget      *m_messagesContainer;
    QVBoxLayout  *m_messagesLayout;

    QComboBox    *m_topicCombo;
    QTextEdit    *m_payloadEdit;
    QPushButton  *m_sendBtn;
    QPushButton  *m_subscribeBtn;

    MqttClient   *m_client;
};

#endif // CHATWIDGET_H
