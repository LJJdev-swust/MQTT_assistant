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
#include <climits>

// ─────────────────────────────────────────────────────
//  性能优化常量
// ─────────────────────────────────────────────────────
// JSON 超过此长度时不进行格式化，直接显示原文以避免 UI 卡顿
static const int kMaxJsonFormatLen = 1024 * 1024; // 1 MB：实际上不做长度限制
// 气泡内容不再截断，完整显示所有数据
static const int kMaxDisplayLen = INT_MAX;

// ─────────────────────────────────────────────────────
//  保序 JSON 缩进格式化
//  不经过 QJsonDocument::toJson()（那会按键名字母序重排字段），
//  而是直接在原始字符串上做字符级缩进展示，完全保留原始字段顺序。
// ─────────────────────────────────────────────────────
static QString indentJsonPreserveOrder(const QString &src)
{
    QString out;
    out.reserve(src.size() * 2);
    int indent = 0;
    bool inString = false;
    bool escape = false;

    auto newline = [&]()
    {
        out += '\n';
        for (int i = 0; i < indent; ++i)
            out += "  ";
    };

    for (int i = 0; i < src.size(); ++i)
    {
        QChar c = src[i];

        if (escape)
        {
            out += c;
            escape = false;
            continue;
        }
        if (inString)
        {
            if (c == '\\')
            {
                out += c;
                escape = true;
                continue;
            }
            if (c == '"')
            {
                out += c;
                inString = false;
                continue;
            }
            out += c;
            continue;
        }

        switch (c.unicode())
        {
        case '"':
            out += c;
            inString = true;
            break;
        case '{':
        case '[':
            out += c;
            ++indent;
            newline();
            break;
        case '}':
        case ']':
            --indent;
            newline();
            out += c;
            break;
        case ',':
            out += c;
            newline();
            break;
        case ':':
            out += ": ";
            break;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            // 跳过原始空白，由我们自己控制缩进
            break;
        default:
            out += c;
        }
    }
    return out;
}

// 根据 dataType 和 payload 返回展示文本
// 使用已知 dataType 可跳过重复的 JSON 解析
static QString formatPayload(const QString &payload, MessageDataType dataType)
{
    if (dataType == MessageDataType::Hex)
        return payload; // HEX 字符串直接显示

    if (dataType == MessageDataType::Json || dataType == MessageDataType::Text)
    {
        // 仅对长度在合理范围内的内容做格式化
        if (payload.size() <= kMaxJsonFormatLen)
        {
            // 先验证是否为合法 JSON
            QJsonParseError err;
            QJsonDocument::fromJson(payload.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError)
            {
                // 使用保序缩进（不经过 Qt JSON 序列化，避免字段重排）
                return indentJsonPreserveOrder(payload.trimmed());
            }
        }
    }
    return payload;
}

// 根据 dataType 返回类型标签（不再重复解析 JSON）
static QString payloadTypeTag(const QString &payload, MessageDataType dataType)
{
    switch (dataType)
    {
    case MessageDataType::Hex:
        return "[HEX]";
    case MessageDataType::Json:
        return "[JSON]";
    default:
        // 兼容旧数据：dataType==Text 但 payload 以 "HEX: " 开头
        if (payload.startsWith("HEX: "))
            return "[HEX]";
        return "[TEXT]";
    }
}

MessageBubbleItem::MessageBubbleItem(const MessageRecord &msg, QWidget *parent)
    : QWidget(parent), m_msg(msg), m_outgoing(msg.outgoing)
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
    QString typeTag = payloadTypeTag(msg.payload, msg.dataType);
    // 注意：不再对显示内容做字符截断，完整展示所有数据

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
    QLabel *tsLabel = new QLabel(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"), bubbleWidget);
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
    if (m_outgoing)
    {
        rowLayout->addStretch();
        rowLayout->addWidget(bubbleWidget);
    }
    else
    {
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
                              "}")
                              .arg(m_bgColor.name(),
                                   m_outgoing ? m_bgColor.name() : "#dddddd");
    bubbleWidget->setStyleSheet(bubbleStyle);

    // Build copy text: topic + payload + timestamp
    m_copyText = QString("[%1] %2\n%3")
                     .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
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
