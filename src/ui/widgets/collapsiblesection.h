#ifndef COLLAPSIBLESECTION_H
#define COLLAPSIBLESECTION_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

/**
 * A collapsible section widget designed for use inside a QSplitter.
 * The header bar is always visible; clicking it (or the arrow button)
 * toggles the content area open/closed.
 * When collapsed the widget reports a fixed height equal to the header,
 * allowing the QSplitter to redistribute space to open siblings.
 */
class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString &title, QWidget *content,
                                QWidget *parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }

    /// Add a widget (e.g., "+" button) to the right side of the header bar.
    void addHeaderWidget(QWidget *w);

signals:
    void toggled(bool expanded);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static const int kHeaderHeight  = 28;
    static const int kMinContentH   = 60;

    QWidget     *m_header;
    QWidget     *m_content;
    QPushButton *m_toggleBtn;
    bool         m_expanded;
};

#endif // COLLAPSIBLESECTION_H
