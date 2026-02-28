#include "messagebubbleitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QContextMenuEvent>

// ─────────────────────────────────────────────────────
//  性能优化常量
// ─────────────────────────────────────────────────────
// JSON 超过此长度时不进行格式化，直接显示原文以避免 UI 卡顿
static const int kMaxJsonFormatLen = 4096;
// 气泡中最多显示的字符数（超出部分截断，防止大量长消息同时渲染）
static const int kMaxDisplayLen    = 2000;

// 根据 dataType 和 payload 返回展示文本
// 使用已知 dataType 可跳过重复的 JSON 解析
static QString formatPayload(const QString &payload, MessageDataType dataType)
{
    if (dataType == MessageDataType::Hex)
        return payload; // HEX 字符串直接显示

    if (dataType == MessageDataType::Json
        || (dataType == MessageDataType::Text && !payload.startsWith("HEX: "))) {
        // 仅对短 JSON 进行格式化
        if (payload.size() <= kMaxJsonFormatLen) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && !doc.isNull())
                return doc.toJson(QJsonDocument::Indented).trimmed();
        }
    }
    return payload;
}

// 根据 dataType 返回类型标签（不再重复解析 JSON）
static QString payloadTypeTag(const QString &payload, MessageDataType dataType)
{
    switch (dataType) {
    case MessageDataType::Hex:  return "[HEX]";
    case MessageDataType::Json: return "[JSON]";
    default:
        // 兼容旧数据：dataType==Text 但 payload 以 "HEX: " 开头
        if (payload.startsWith("HEX: "))
            return "[HEX]";
        return "[TEXT]";
    }
}

MessageBubbleItem::MessageBubbleItem(const MessageRecord &msg, QWidget *parent)
    : QWidget(parent)
    , m_msg(msg)
    , m_outgoing(msg.outgoing)
{
    m_bgColor = m_outgoing ? QColor("#ea5413") : QColor("#ffffff");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setContextMenuPolicy(Qt::DefaultContextMenu);

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

    // Format detection — 使用 msg.dataType 避免重复解析
    QString displayPayload = formatPayload(msg.payload, msg.dataType);
    QString typeTag        = payloadTypeTag(msg.payload, msg.dataType);

    // 截断过长的显示内容（原始数据保留在 m_msg.payload 中，右键可复制完整内容）
    bool truncated = false;
    if (displayPayload.size() > kMaxDisplayLen) {
        displayPayload = displayPayload.left(kMaxDisplayLen) + "\n... [内容已截断，右键→复制内容 可获取完整数据]";
        truncated = true;
    }
    Q_UNUSED(truncated)

    // Type tag label
    QString retainedTag = msg.retained ? " [留存]" : "";
    QLabel *typeLabel = new QLabel(typeTag + retainedTag, bubbleWidget);
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
    QLabel *tsLabel = new QLabel(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss"), bubbleWidget);
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

    // Build copy text: topic + payload + timestamp
    m_copyText = QString("[%1] %2\n%3")
                     .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                     .arg(msg.topic)
                     .arg(msg.payload);
}

void MessageBubbleItem::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *actCopy = menu.addAction("复制");
    QAction *actCopyTopic = menu.addAction("复制主题");
    QAction *actCopyPayload = menu.addAction("复制内容");
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == actCopy)
        QApplication::clipboard()->setText(m_copyText);
    else if (chosen == actCopyTopic)
        QApplication::clipboard()->setText(m_msg.topic);
    else if (chosen == actCopyPayload)
        QApplication::clipboard()->setText(m_msg.payload);
}
