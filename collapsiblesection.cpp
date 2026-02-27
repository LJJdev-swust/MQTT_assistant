#include "collapsiblesection.h"
#include <QEvent>
#include <QMouseEvent>

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *content,
                                       QWidget *parent)
    : QWidget(parent)
    , m_content(content)
    , m_expanded(true)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Header bar ----
    m_header = new QWidget(this);
    m_header->setObjectName("collapsibleHeader");
    m_header->setFixedHeight(kHeaderHeight);
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->installEventFilter(this);

    QHBoxLayout *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(6, 0, 6, 0);
    headerLayout->setSpacing(4);

    m_toggleBtn = new QPushButton("▼", m_header);
    m_toggleBtn->setObjectName("btnCollapse");
    m_toggleBtn->setFixedSize(16, 16);
    m_toggleBtn->setFlat(true);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);

    QLabel *titleLabel = new QLabel(title, m_header);
    titleLabel->setObjectName("labelSectionTitle");

    headerLayout->addWidget(m_toggleBtn);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    mainLayout->addWidget(m_header);

    // ---- Content ----
    m_content->setParent(this);
    mainLayout->addWidget(m_content, 1);

    connect(m_toggleBtn, &QPushButton::clicked,
            this, [this]() { setExpanded(!m_expanded); });

    setExpanded(true);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_content->setVisible(expanded);
    m_toggleBtn->setText(expanded ? "▼" : "▶");

    if (expanded) {
        setMinimumHeight(kHeaderHeight + kMinContentH);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setFixedHeight(kHeaderHeight);
    }

    emit toggled(expanded);
}

void CollapsibleSection::addHeaderWidget(QWidget *w)
{
    auto *hl = qobject_cast<QHBoxLayout *>(m_header->layout());
    if (hl)
        hl->addWidget(w);
}

bool CollapsibleSection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        // Only toggle when releasing on the header (not on a child button)
        if (me->button() == Qt::LeftButton)
            setExpanded(!m_expanded);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
