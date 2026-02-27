#include "scriptdialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>

ScriptDialog::ScriptDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(ScriptConfig());
    setWindowTitle("新建脚本");
}

ScriptDialog::ScriptDialog(const ScriptConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(config);
    setWindowTitle("编辑脚本");
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
    m_nameEdit->setPlaceholderText("脚本名称");
    form->addRow("名称:", m_nameEdit);

    m_enabledCheck = new QCheckBox("已启用", this);
    m_enabledCheck->setChecked(true);
    form->addRow("状态:", m_enabledCheck);

    m_triggerTopicEdit = new QLineEdit(this);
    m_triggerTopicEdit->setPlaceholderText("如: sensors/# (空=任意)");
    form->addRow("触发主题:", m_triggerTopicEdit);

    m_conditionCombo = new QComboBox(this);
    m_conditionCombo->addItem("任意",        "any");
    m_conditionCombo->addItem("包含",   "contains");
    m_conditionCombo->addItem("等于",     "equals");
    m_conditionCombo->addItem("开头匹配", "startsWith");
    m_conditionCombo->addItem("结尾匹配",   "endsWith");
    m_conditionCombo->addItem("正则表达式",      "regex");
    form->addRow("触发条件:", m_conditionCombo);

    m_triggerValueEdit = new QLineEdit(this);
    m_triggerValueEdit->setPlaceholderText("匹配值...");
    m_triggerValueEdit->setEnabled(false);
    form->addRow("匹配值:", m_triggerValueEdit);

    m_responseTopicEdit = new QLineEdit(this);
    m_responseTopicEdit->setPlaceholderText("如: actuators/result");
    form->addRow("响应主题:", m_responseTopicEdit);

    m_responsePayloadEdit = new QTextEdit(this);
    m_responsePayloadEdit->setPlaceholderText(
        "响应内容... 支持 {{timestamp}} {{topic}} {{payload}}");
    m_responsePayloadEdit->setMaximumHeight(80);
    form->addRow("响应内容:", m_responsePayloadEdit);

    m_responseQosCombo = new QComboBox(this);
    m_responseQosCombo->addItem("0 - 最多一次");
    m_responseQosCombo->addItem("1 - 至少一次");
    m_responseQosCombo->addItem("2 - 恰好一次");
    form->addRow("响应 QoS:", m_responseQosCombo);

    m_responseRetainCheck = new QCheckBox("保留响应", this);
    form->addRow("保留:", m_responseRetainCheck);

    m_delayMsSpin = new QSpinBox(this);
    m_delayMsSpin->setRange(0, 60000);
    m_delayMsSpin->setValue(0);
    m_delayMsSpin->setSuffix(" ms");
    form->addRow("延迟:", m_delayMsSpin);

    mainLayout->addLayout(form);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText("确定");
    bbox->button(QDialogButtonBox::Cancel)->setText("取消");
    mainLayout->addWidget(bbox);

    connect(bbox,             &QDialogButtonBox::accepted, this, [this]() {
        // Validate response topic: '#' is not allowed in publish topics
        if (m_responseTopicEdit->text().contains('#')) {
            QMessageBox::warning(this, "主题格式错误",
                                 "响应主题（发布主题）不能包含通配符 '#'，请修正后重试。");
            return;
        }
        accept();
    });
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
