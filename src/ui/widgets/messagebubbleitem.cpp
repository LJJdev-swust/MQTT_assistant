#include "messagebubbleitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonParseError>

// Returns a display-friendly representation of a raw payload string.
static QString formatPayload(const QString &payload)
{
    // Try JSON
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull())
        return doc.toJson(QJsonDocument::Indented).trimmed();
    return payload;
}

// Returns a short type tag for the payload.
static QString payloadTypeTag(const QString &payload)
{
    if (payload.startsWith("HEX: "))
        return "[HEX]";
    QJsonParseError err;
    QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError)
        return "[JSON]";
    return "[TEXT]";
}

MessageBubbleItem::MessageBubbleItem(const MessageRecord &msg, QWidget *parent)
    : QWidget(parent)
    , m_msg(msg)
    , m_outgoing(msg.outgoing)
{
    m_bgColor = m_outgoing ? QColor("#ea5413") : QColor("#ffffff");
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
        : "color: #1e1e2e; background: transparent;");
    topicLabel->setWordWrap(true);

    // Format detection
    QString displayPayload = formatPayload(msg.payload);
    QString typeTag = payloadTypeTag(msg.payload);

    // Type tag label
    QLabel *typeLabel = new QLabel(typeTag, bubbleWidget);
    QFont typeFont = typeLabel->font();
    typeFont.setPointSize(typeFont.pointSize() - 2);
    typeFont.setBold(true);
    typeLabel->setFont(typeFont);
    typeLabel->setStyleSheet(m_outgoing
        ? "color: rgba(255,255,255,0.85); background: transparent;"
        : "color: #f39800; background: transparent;");

    // Payload label
    QLabel *payloadLabel = new QLabel(displayPayload, bubbleWidget);
    payloadLabel->setWordWrap(true);
    payloadLabel->setTextFormat(Qt::PlainText);
    payloadLabel->setStyleSheet(m_outgoing
        ? "color: #fff5f0; background: transparent;"
        : "color: #333333; background: transparent;");

    // Timestamp label
    QLabel *tsLabel = new QLabel(msg.timestamp.toString("hh:mm:ss"), bubbleWidget);
    QFont tsFont = tsLabel->font();
    tsFont.setPointSize(tsFont.pointSize() - 2);
    tsLabel->setFont(tsFont);
    tsLabel->setStyleSheet(m_outgoing
        ? "color: rgba(255,255,255,0.7); background: transparent;"
        : "color: #888888; background: transparent;");
    tsLabel->setAlignment(m_outgoing ? Qt::AlignRight : Qt::AlignLeft);

    bubbleLayout->addWidget(topicLabel);
    bubbleLayout->addWidget(typeLabel);
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
        "  border: 1px solid %2;"
        "}"
    ).arg(m_bgColor.name(),
          m_outgoing ? m_bgColor.name() : "#dddddd");
    bubbleWidget->setStyleSheet(bubbleStyle);
}
