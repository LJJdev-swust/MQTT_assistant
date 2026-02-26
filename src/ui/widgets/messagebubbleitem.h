#ifndef MESSAGEBUBBLEITEM_H
#define MESSAGEBUBBLEITEM_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "core/models.h"

class MessageBubbleItem : public QWidget
{
    Q_OBJECT
public:
    explicit MessageBubbleItem(const MessageRecord &msg, QWidget *parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    MessageRecord m_msg;
    QColor m_bgColor;
    bool m_outgoing;
    QString m_copyText; // full text to copy
};

#endif // MESSAGEBUBBLEITEM_H
