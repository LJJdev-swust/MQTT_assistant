#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
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

    // Persist/restore topic history to QSettings
    void saveTopicHistory();
    void loadTopicHistory();

signals:
    void sendRequested(const QString &topic, const QString &payload);
    void subscribeRequested(const QString &topic);
    void clearHistoryRequested(int connectionId); // emitted when user wants DB clear

private slots:
    void onSendClicked();
    void onSubscribeClicked();
    void onClearClicked();
    void scrollToBottom();

private:
    QScrollArea  *m_scrollArea;
    QWidget      *m_messagesContainer;
    QVBoxLayout  *m_messagesLayout;
    QSplitter    *m_splitter;

    QComboBox    *m_topicCombo;
    QTextEdit    *m_payloadEdit;
    QPushButton  *m_sendBtn;
    QPushButton  *m_subscribeBtn;

    MqttClient   *m_client;
    int           m_connectionId; // for DB clear

    static const int kMaxTopicHistory = 10;
};

#endif // CHATWIDGET_H
