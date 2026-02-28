#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QLineEdit>
#include <QList>
#include <QEvent>
#include "core/models.h"

class MqttClient;

class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void setClient(MqttClient *client);
    void setConnectionId(int id) { m_connectionId = id; }
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
    void clearHistoryRequested(int connectionId);    // emitted when user wants DB clear
    void displayClearedRequested(int connectionId);  // emitted whenever display is cleared
public slots:
    void onClearClicked();
    void scrollToBottom();

private slots:
    void onSendClicked();
    void onSubscribeClicked();
    void onScrollValueChanged(int value);

private:
    void updateScrollToBottomBtn();
    void repositionScrollToBottomBtn();
    void processNextBatch(int generation);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QScrollArea  *m_scrollArea;
    QWidget      *m_messagesContainer;
    QVBoxLayout  *m_messagesLayout;
    QSplitter    *m_splitter;

    QComboBox    *m_topicCombo;
    QTextEdit    *m_payloadEdit;
    QPushButton  *m_sendBtn;
    QPushButton  *m_subscribeBtn;
    QPushButton  *m_scrollToBottomBtn;  // floating "scroll to bottom" button

    MqttClient   *m_client;
    int           m_connectionId; // for DB clear
    int           m_pendingScrollCount; // new messages received while scrolled up

    // Batch message loading (avoids blocking the UI thread)
    QList<MessageRecord> m_loadQueue;
    int           m_loadGeneration;     // incremented each time loadMessages() is called

    static const int kMaxTopicHistory = 10;
    static const int kLoadBatchSize   = 20;
};

#endif // CHATWIDGET_H
