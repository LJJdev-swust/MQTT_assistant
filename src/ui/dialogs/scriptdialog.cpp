#include "scriptdialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

ScriptDialog::ScriptDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(ScriptConfig());
    setWindowTitle("New Script");
}

ScriptDialog::ScriptDialog(const ScriptConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(config);
    setWindowTitle("Edit Script");
}

void ScriptDialog::setupUi()
{
    setMinimumWidth(460);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Script name");
    form->addRow("Name:", m_nameEdit);

    m_enabledCheck = new QCheckBox("Enabled", this);
    m_enabledCheck->setChecked(true);
    form->addRow("Status:", m_enabledCheck);

    m_triggerTopicEdit = new QLineEdit(this);
    m_triggerTopicEdit->setPlaceholderText("e.g. sensors/# (empty = any)");
    form->addRow("Trigger Topic:", m_triggerTopicEdit);

    m_conditionCombo = new QComboBox(this);
    m_conditionCombo->addItem("Any",        "any");
    m_conditionCombo->addItem("Contains",   "contains");
    m_conditionCombo->addItem("Equals",     "equals");
    m_conditionCombo->addItem("StartsWith", "startsWith");
    m_conditionCombo->addItem("EndsWith",   "endsWith");
    m_conditionCombo->addItem("RegEx",      "regex");
    form->addRow("Condition:", m_conditionCombo);

    m_triggerValueEdit = new QLineEdit(this);
    m_triggerValueEdit->setPlaceholderText("Match value...");
    m_triggerValueEdit->setEnabled(false);
    form->addRow("Match Value:", m_triggerValueEdit);

    m_responseTopicEdit = new QLineEdit(this);
    m_responseTopicEdit->setPlaceholderText("e.g. actuators/result");
    form->addRow("Response Topic:", m_responseTopicEdit);

    m_responsePayloadEdit = new QTextEdit(this);
    m_responsePayloadEdit->setPlaceholderText(
        "Response payload... Supports {{timestamp}} {{topic}} {{payload}}");
    m_responsePayloadEdit->setMaximumHeight(80);
    form->addRow("Response Payload:", m_responsePayloadEdit);

    m_responseQosCombo = new QComboBox(this);
    m_responseQosCombo->addItem("0 - At most once");
    m_responseQosCombo->addItem("1 - At least once");
    m_responseQosCombo->addItem("2 - Exactly once");
    form->addRow("Response QoS:", m_responseQosCombo);

    m_responseRetainCheck = new QCheckBox("Retain response", this);
    form->addRow("Retain:", m_responseRetainCheck);

    m_delayMsSpin = new QSpinBox(this);
    m_delayMsSpin->setRange(0, 60000);
    m_delayMsSpin->setValue(0);
    m_delayMsSpin->setSuffix(" ms");
    form->addRow("Delay:", m_delayMsSpin);

    mainLayout->addLayout(form);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(bbox);

    connect(bbox,             &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox,             &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_conditionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScriptDialog::onConditionChanged);
}

void ScriptDialog::populateFrom(const ScriptConfig &config)
{
    m_nameEdit->setText(config.name);
    m_enabledCheck->setChecked(config.enabled);
    m_triggerTopicEdit->setText(config.triggerTopic);

    // Select correct condition
    int condIdx = m_conditionCombo->findData(config.triggerCondition);
    if (condIdx >= 0)
        m_conditionCombo->setCurrentIndex(condIdx);

    m_triggerValueEdit->setText(config.triggerValue);
    m_triggerValueEdit->setEnabled(config.triggerCondition != "any");
    m_responseTopicEdit->setText(config.responseTopic);
    m_responsePayloadEdit->setPlainText(config.responsePayload);
    m_responseQosCombo->setCurrentIndex(qBound(0, config.responseQos, 2));
    m_responseRetainCheck->setChecked(config.responseRetain);
    m_delayMsSpin->setValue(config.delayMs);
}

ScriptConfig ScriptDialog::config() const
{
    ScriptConfig s;
    s.name             = m_nameEdit->text().trimmed();
    s.enabled          = m_enabledCheck->isChecked();
    s.triggerTopic     = m_triggerTopicEdit->text().trimmed();
    s.triggerCondition = m_conditionCombo->currentData().toString();
    s.triggerValue     = m_triggerValueEdit->text();
    s.responseTopic    = m_responseTopicEdit->text().trimmed();
    s.responsePayload  = m_responsePayloadEdit->toPlainText();
    s.responseQos      = m_responseQosCombo->currentIndex();
    s.responseRetain   = m_responseRetainCheck->isChecked();
    s.delayMs          = m_delayMsSpin->value();
    return s;
}

void ScriptDialog::onConditionChanged(int index)
{
    QString cond = m_conditionCombo->itemData(index).toString();
    m_triggerValueEdit->setEnabled(cond != "any");
}
