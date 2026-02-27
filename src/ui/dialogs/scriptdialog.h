#ifndef SCRIPTDIALOG_H
#define SCRIPTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include "core/models.h"

class ScriptDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScriptDialog(QWidget *parent = nullptr);
    explicit ScriptDialog(const ScriptConfig &config, QWidget *parent = nullptr);

    ScriptConfig config() const;

private slots:
    void onConditionChanged(int index);

private:
    void setupUi();
    void populateFrom(const ScriptConfig &config);

    QLineEdit  *m_nameEdit;
    QCheckBox  *m_enabledCheck;
    QLineEdit  *m_triggerTopicEdit;
    QComboBox  *m_conditionCombo;
    QLineEdit  *m_triggerValueEdit;
    QLineEdit  *m_responseTopicEdit;
    QTextEdit  *m_responsePayloadEdit;
    QComboBox  *m_responseQosCombo;
    QCheckBox  *m_responseRetainCheck;
    QSpinBox   *m_delayMsSpin;
};

#endif // SCRIPTDIALOG_H
