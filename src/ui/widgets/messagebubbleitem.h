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

private:
    MessageRecord m_msg;
    QColor m_bgColor;
    bool m_outgoing;
};

#endif // MESSAGEBUBBLEITEM_H
