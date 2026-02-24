#include "messagebubbleitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

MessageBubbleItem::MessageBubbleItem(const MessageRecord &msg, QWidget *parent)
    : QWidget(parent)
    , m_msg(msg)
    , m_outgoing(msg.outgoing)
{
    m_bgColor = m_outgoing ? QColor("#ea5413") : QColor("#7f7f80");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 4, 8, 4);
    outerLayout->setSpacing(0);

    // Bubble container
    QWidget *bubbleWidget = new QWidget(this);
    bubbleWidget->setObjectName("bubbleWidget");
    bubbleWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleWidget);
    bubbleLayout->setContentsMargins(12, 8, 12, 8);
    bubbleLayout->setSpacing(4);

    // Topic label (bold)
    QLabel *topicLabel = new QLabel(msg.topic, bubbleWidget);
    QFont topicFont = topicLabel->font();
    topicFont.setBold(true);
    topicLabel->setFont(topicFont);
    topicLabel->setStyleSheet(m_outgoing
        ? "color: #ffffff; background: transparent;"
        : "color: #e0e0e0; background: transparent;");
    topicLabel->setWordWrap(true);

    // Payload label
    QLabel *payloadLabel = new QLabel(msg.payload, bubbleWidget);
    payloadLabel->setWordWrap(true);
    payloadLabel->setStyleSheet(m_outgoing
        ? "color: #fff5f0; background: transparent;"
        : "color: #d0d0d0; background: transparent;");

    // Timestamp label
    QLabel *tsLabel = new QLabel(msg.timestamp.toString("hh:mm:ss"), bubbleWidget);
    QFont tsFont = tsLabel->font();
    tsFont.setPointSize(tsFont.pointSize() - 2);
    tsLabel->setFont(tsFont);
    tsLabel->setStyleSheet(m_outgoing
        ? "color: rgba(255,255,255,0.7); background: transparent;"
        : "color: rgba(220,220,220,0.6); background: transparent;");
    tsLabel->setAlignment(m_outgoing ? Qt::AlignRight : Qt::AlignLeft);

    bubbleLayout->addWidget(topicLabel);
    bubbleLayout->addWidget(payloadLabel);
    bubbleLayout->addWidget(tsLabel);

    // Align bubble
    QHBoxLayout *rowLayout = new QHBoxLayout();
    rowLayout->setContentsMargins(0, 0, 0, 0);
    if (m_outgoing) {
        rowLayout->addStretch();
        rowLayout->addWidget(bubbleWidget);
    } else {
        rowLayout->addWidget(bubbleWidget);
        rowLayout->addStretch();
    }
    outerLayout->addLayout(rowLayout);

    // Style bubble background
    QString bubbleStyle = QString(
        "QWidget#bubbleWidget {"
        "  background-color: %1;"
        "  border-radius: 12px;"
        "}"
    ).arg(m_bgColor.name());
    bubbleWidget->setStyleSheet(bubbleStyle);
}
