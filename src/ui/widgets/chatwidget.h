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
#include <QLabel>
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
    void unfreezeMessages(); // re-enable painting after a standalone clearMessages() call
    void loadMessages(const QList<MessageRecord> &messages);
    // Prepend older messages at the top (called by MainWindow for lazy loading)
    void prependMessages(const QList<MessageRecord> &messages);

    // Show/hide the loading overlay (spinner)
    void showLoadingOverlay();
    void hideLoadingOverlay();

    QComboBox *topicCombo() const { return m_topicCombo; }

    // Persist/restore topic history to QSettings
    void saveTopicHistory();
    void loadTopicHistory();

signals:
    void sendRequested(const QString &topic, const QString &payload);
    void subscribeRequested(const QString &topic);
    void clearHistoryRequested(int connectionId);             // emitted when user wants DB clear
    void displayClearedRequested(int connectionId);           // emitted whenever display is cleared
    void requestMoreMessages(int connectionId, int oldestId); // lazy load older messages
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
    void repositionLoadingOverlay();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QScrollArea *m_scrollArea;
    QWidget *m_messagesContainer;
    QVBoxLayout *m_messagesLayout;
    QSplitter *m_splitter;

    QComboBox *m_topicCombo;
    QTextEdit *m_payloadEdit;
    QPushButton *m_sendBtn;
    QPushButton *m_clearPayloadBtn; // clears the payload input manually
    QPushButton *m_subscribeBtn;
    QPushButton *m_scrollToBottomBtn; // floating "scroll to bottom" button

    // Loading overlay (shown during async history load)
    QWidget *m_loadingOverlay;
    QLabel *m_loadingSpinner;
    int m_spinnerAngle; // current rotation angle for custom spinner
    QTimer *m_spinnerTimer;

    MqttClient *m_client;
    int m_connectionId;       // for DB clear
    int m_pendingScrollCount; // new messages received while scrolled up

    // Batch message loading (avoids blocking the UI thread)
    QList<MessageRecord> m_loadQueue;
    int m_loadGeneration; // incremented each time loadMessages() is called

    // Lazy / infinite-scroll state
    int m_oldestLoadedId;        // DB id of the oldest message currently shown (-1 = unknown)
    bool m_allHistoryLoaded;     // true once server says no more older messages
    bool m_loadingOlderMessages; // true while an async older-message request is in flight

    // Drag-state for prepend scroll-jump suppression
    bool m_sliderBeingDragged; // true while user drags the scroll bar thumb

    static const int kMaxTopicHistory = 10;
    static const int kLoadBatchSize = 20; // bubbles rendered per event-loop tick
    static const int kLazyPageSize = 50;  // rows fetched per "load older" request (matches SQL LIMIT)
};

#endif // CHATWIDGET_H
