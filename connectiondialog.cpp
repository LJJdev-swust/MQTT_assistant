#include "connectiondialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QUuid>
#include <QLabel>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    MqttConnectionConfig defaults;
    populateFrom(defaults);
    setWindowTitle("新建连接");
}

ConnectionDialog::ConnectionDialog(const MqttConnectionConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(config);
    setWindowTitle("编辑连接");
}

void ConnectionDialog::setupUi()
{
    setMinimumWidth(480);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("我的服务器");
    form->addRow("名称:", m_nameEdit);

    // Scheme + Host row
    QHBoxLayout *hostRow = new QHBoxLayout();
    hostRow->setSpacing(4);
    m_schemeCombo = new QComboBox(this);
    m_schemeCombo->addItem("mqtt://");
    m_schemeCombo->addItem("mqtts://");
    m_schemeCombo->setFixedWidth(88);
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("localhost");
    hostRow->addWidget(m_schemeCombo);
    hostRow->addWidget(m_hostEdit);
    form->addRow("地址:", hostRow);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(1883);
    form->addRow("端口:", m_portSpin);

    m_usernameEdit = new QLineEdit(this);
    form->addRow("用户名:", m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow("密码:", m_passwordEdit);

    QHBoxLayout *cidRow = new QHBoxLayout();
    m_clientIdEdit = new QLineEdit(this);
    m_generateBtn  = new QPushButton("生成", this);
    m_generateBtn->setFixedWidth(60);
    cidRow->addWidget(m_clientIdEdit);
    cidRow->addWidget(m_generateBtn);
    form->addRow("客户端ID:", cidRow);

    m_cleanSessionCheck = new QCheckBox("清除会话", this);
    m_cleanSessionCheck->setChecked(true);
    form->addRow("", m_cleanSessionCheck);

    m_keepAliveSpin = new QSpinBox(this);
    m_keepAliveSpin->setRange(0, 65535);
    m_keepAliveSpin->setValue(60);
    m_keepAliveSpin->setSuffix(" 秒");
    form->addRow("心跳间隔:", m_keepAliveSpin);

    mainLayout->addLayout(form);

    // TLS group
    m_tlsGroup = new QGroupBox("TLS / SSL 加密", this);
    m_tlsGroup->setCheckable(true);
    m_tlsGroup->setChecked(false);
    QFormLayout *tlsForm = new QFormLayout(m_tlsGroup);
    tlsForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tlsForm->setSpacing(6);

    auto makeFileRow = [&](const QString &label, QLineEdit *&edit, QPushButton *&btn) {
        QHBoxLayout *row = new QHBoxLayout();
        edit = new QLineEdit(m_tlsGroup);
        btn  = new QPushButton("浏览", m_tlsGroup);
        btn->setFixedWidth(60);
        row->addWidget(edit);
        row->addWidget(btn);
        tlsForm->addRow(label, row);
    };

    makeFileRow("CA 证书:",      m_caCertEdit,     m_caCertBtn);
    makeFileRow("客户端证书:",  m_clientCertEdit, m_clientCertBtn);
    makeFileRow("客户端密钥:",   m_clientKeyEdit,  m_clientKeyBtn);

    mainLayout->addWidget(m_tlsGroup);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText("确定");
    bbox->button(QDialogButtonBox::Cancel)->setText("取消");
    mainLayout->addWidget(bbox);

    connect(bbox,           &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox,           &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_generateBtn,  &QPushButton::clicked,       this, &ConnectionDialog::generateClientId);
    connect(m_caCertBtn,    &QPushButton::clicked,       this, &ConnectionDialog::browseCaCert);
    connect(m_clientCertBtn,&QPushButton::clicked,       this, &ConnectionDialog::browseClientCert);
    connect(m_clientKeyBtn, &QPushButton::clicked,       this, &ConnectionDialog::browseClientKey);
    connect(m_schemeCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::onSchemeChanged);
}

void ConnectionDialog::onSchemeChanged(int index)
{
    bool isTls = (index == 1); // "mqtts://"
    m_tlsGroup->setChecked(isTls);
    if (isTls && m_portSpin->value() == 1883)
        m_portSpin->setValue(8883);
    else if (!isTls && m_portSpin->value() == 8883)
        m_portSpin->setValue(1883);
}

void ConnectionDialog::populateFrom(const MqttConnectionConfig &config)
{
    m_nameEdit->setText(config.name);
    m_hostEdit->setText(config.host.isEmpty() ? "localhost" : config.host);
    m_portSpin->setValue(config.port > 0 ? config.port : 1883);
    m_usernameEdit->setText(config.username);
    m_passwordEdit->setText(config.password);
    m_clientIdEdit->setText(config.clientId);
    m_cleanSessionCheck->setChecked(config.cleanSession);
    m_keepAliveSpin->setValue(config.keepAlive > 0 ? config.keepAlive : 60);
    // Set scheme based on TLS flag
    m_schemeCombo->blockSignals(true);
    m_schemeCombo->setCurrentIndex(config.useTLS ? 1 : 0);
    m_schemeCombo->blockSignals(false);
    m_tlsGroup->setChecked(config.useTLS);
    m_caCertEdit->setText(config.caCertPath);
    m_clientCertEdit->setText(config.clientCertPath);
    m_clientKeyEdit->setText(config.clientKeyPath);
}

MqttConnectionConfig ConnectionDialog::config() const
{
    MqttConnectionConfig c;
    c.name           = m_nameEdit->text().trimmed();
    c.host           = m_hostEdit->text().trimmed();
    c.port           = m_portSpin->value();
    c.username       = m_usernameEdit->text();
    c.password       = m_passwordEdit->text();
    c.clientId       = m_clientIdEdit->text().trimmed();
    c.cleanSession   = m_cleanSessionCheck->isChecked();
    c.keepAlive      = m_keepAliveSpin->value();
    // useTLS is driven by either the scheme combo or the TLS group checkbox
    c.useTLS         = (m_schemeCombo->currentIndex() == 1) || m_tlsGroup->isChecked();
    c.caCertPath     = m_caCertEdit->text().trimmed();
    c.clientCertPath = m_clientCertEdit->text().trimmed();
    c.clientKeyPath  = m_clientKeyEdit->text().trimmed();
    return c;
}

void ConnectionDialog::generateClientId()
{
    QString uuid = QUuid::createUuid().toString();
    uuid.remove('{').remove('}');
    m_clientIdEdit->setText("mqtt_" + uuid.left(8));
}

void ConnectionDialog::browseCaCert()
{
    QString path = QFileDialog::getOpenFileName(this, "选择 CA 证书",
                                                QString(), "PEM 文件 (*.pem *.crt *.cer);;所有文件 (*)");
    if (!path.isEmpty())
        m_caCertEdit->setText(path);
}

void ConnectionDialog::browseClientCert()
{
    QString path = QFileDialog::getOpenFileName(this, "选择客户端证书",
                                                QString(), "PEM 文件 (*.pem *.crt *.cer);;所有文件 (*)");
    if (!path.isEmpty())
        m_clientCertEdit->setText(path);
}

void ConnectionDialog::browseClientKey()
{
    QString path = QFileDialog::getOpenFileName(this, "选择客户端密钥",
                                                QString(), "PEM 文件 (*.pem *.key);;所有文件 (*)");
    if (!path.isEmpty())
        m_clientKeyEdit->setText(path);
}
