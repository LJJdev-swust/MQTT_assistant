#include "commanddialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

CommandDialog::CommandDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(CommandConfig());
    setWindowTitle("New Command");
}

CommandDialog::CommandDialog(const CommandConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(config);
    setWindowTitle("Edit Command");
}

void CommandDialog::setupUi()
{
    setMinimumWidth(400);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Command name");
    form->addRow("Name:", m_nameEdit);

    m_topicEdit = new QLineEdit(this);
    m_topicEdit->setPlaceholderText("e.g. home/lights/on");
    form->addRow("Topic:", m_topicEdit);

    m_payloadEdit = new QTextEdit(this);
    m_payloadEdit->setPlaceholderText("Message payload...");
    m_payloadEdit->setMaximumHeight(80);
    form->addRow("Payload:", m_payloadEdit);

    m_qosCombo = new QComboBox(this);
    m_qosCombo->addItem("0 - At most once");
    m_qosCombo->addItem("1 - At least once");
    m_qosCombo->addItem("2 - Exactly once");
    form->addRow("QoS:", m_qosCombo);

    m_retainCheck = new QCheckBox("Retain message", this);
    form->addRow("Retain:", m_retainCheck);

    m_loopCheck = new QCheckBox("Enable loop", this);
    form->addRow("Loop:", m_loopCheck);

    QHBoxLayout *intervalRow = new QHBoxLayout();
    m_loopIntervalSpin = new QSpinBox(this);
    m_loopIntervalSpin->setRange(100, 3600000);
    m_loopIntervalSpin->setValue(1000);
    m_loopIntervalSpin->setSuffix(" ms");
    m_loopIntervalSpin->setEnabled(false);
    QLabel *intervalLabel = new QLabel("Interval:", this);
    intervalRow->addWidget(intervalLabel);
    intervalRow->addWidget(m_loopIntervalSpin);
    intervalRow->addStretch();
    form->addRow("", intervalRow);

    mainLayout->addLayout(form);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(bbox);

    connect(bbox,        &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox,        &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_loopCheck, &QCheckBox::toggled,         this, &CommandDialog::onLoopChecked);
}

void CommandDialog::populateFrom(const CommandConfig &config)
{
    m_nameEdit->setText(config.name);
    m_topicEdit->setText(config.topic);
    m_payloadEdit->setPlainText(config.payload);
    m_qosCombo->setCurrentIndex(qBound(0, config.qos, 2));
    m_retainCheck->setChecked(config.retain);
    m_loopCheck->setChecked(config.loopEnabled);
    m_loopIntervalSpin->setValue(config.loopIntervalMs > 0 ? config.loopIntervalMs : 1000);
    m_loopIntervalSpin->setEnabled(config.loopEnabled);
}

CommandConfig CommandDialog::config() const
{
    CommandConfig c;
    c.name           = m_nameEdit->text().trimmed();
    c.topic          = m_topicEdit->text().trimmed();
    c.payload        = m_payloadEdit->toPlainText();
    c.qos            = m_qosCombo->currentIndex();
    c.retain         = m_retainCheck->isChecked();
    c.loopEnabled    = m_loopCheck->isChecked();
    c.loopIntervalMs = m_loopIntervalSpin->value();
    return c;
}

void CommandDialog::onLoopChecked(bool checked)
{
    m_loopIntervalSpin->setEnabled(checked);
}
