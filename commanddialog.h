#ifndef COMMANDDIALOG_H
#define COMMANDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include "core/models.h"

class CommandDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CommandDialog(QWidget *parent = nullptr);
    explicit CommandDialog(const CommandConfig &config, QWidget *parent = nullptr);

    CommandConfig config() const;

private slots:
    void onLoopChecked(bool checked);

private:
    void setupUi();
    void populateFrom(const CommandConfig &config);

    QLineEdit  *m_nameEdit;
    QLineEdit  *m_topicEdit;
    QTextEdit  *m_payloadEdit;
    QComboBox  *m_qosCombo;
    QCheckBox  *m_retainCheck;
    QCheckBox  *m_loopCheck;
    QSpinBox   *m_loopIntervalSpin;
};

#endif // COMMANDDIALOG_H
