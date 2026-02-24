#include "connectiondialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QUuid>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    MqttConnectionConfig defaults;
    populateFrom(defaults);
    setWindowTitle("New Connection");
}

ConnectionDialog::ConnectionDialog(const MqttConnectionConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populateFrom(config);
    setWindowTitle("Edit Connection");
}

void ConnectionDialog::setupUi()
{
    setMinimumWidth(460);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("My Broker");
    form->addRow("Name:", m_nameEdit);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("localhost");
    form->addRow("Host:", m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(1883);
    form->addRow("Port:", m_portSpin);

    m_usernameEdit = new QLineEdit(this);
    form->addRow("Username:", m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow("Password:", m_passwordEdit);

    QHBoxLayout *cidRow = new QHBoxLayout();
    m_clientIdEdit = new QLineEdit(this);
    m_generateBtn  = new QPushButton("Generate", this);
    m_generateBtn->setFixedWidth(80);
    cidRow->addWidget(m_clientIdEdit);
    cidRow->addWidget(m_generateBtn);
    form->addRow("Client ID:", cidRow);

    m_cleanSessionCheck = new QCheckBox("Clean Session", this);
    m_cleanSessionCheck->setChecked(true);
    form->addRow("", m_cleanSessionCheck);

    m_keepAliveSpin = new QSpinBox(this);
    m_keepAliveSpin->setRange(0, 65535);
    m_keepAliveSpin->setValue(60);
    m_keepAliveSpin->setSuffix(" s");
    form->addRow("Keep Alive:", m_keepAliveSpin);

    mainLayout->addLayout(form);

    // TLS group
    m_tlsGroup = new QGroupBox("TLS / SSL", this);
    m_tlsGroup->setCheckable(true);
    m_tlsGroup->setChecked(false);
    QFormLayout *tlsForm = new QFormLayout(m_tlsGroup);
    tlsForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tlsForm->setSpacing(6);

    auto makeFileRow = [&](const QString &label, QLineEdit *&edit, QPushButton *&btn) {
        QHBoxLayout *row = new QHBoxLayout();
        edit = new QLineEdit(m_tlsGroup);
        btn  = new QPushButton("Browse", m_tlsGroup);
        btn->setFixedWidth(70);
        row->addWidget(edit);
        row->addWidget(btn);
        tlsForm->addRow(label, row);
    };

    makeFileRow("CA Cert:",      m_caCertEdit,     m_caCertBtn);
    makeFileRow("Client Cert:",  m_clientCertEdit, m_clientCertBtn);
    makeFileRow("Client Key:",   m_clientKeyEdit,  m_clientKeyBtn);

    mainLayout->addWidget(m_tlsGroup);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(bbox);

    connect(bbox,           &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox,           &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_generateBtn,  &QPushButton::clicked,       this, &ConnectionDialog::generateClientId);
    connect(m_caCertBtn,    &QPushButton::clicked,       this, &ConnectionDialog::browseCaCert);
    connect(m_clientCertBtn,&QPushButton::clicked,       this, &ConnectionDialog::browseClientCert);
    connect(m_clientKeyBtn, &QPushButton::clicked,       this, &ConnectionDialog::browseClientKey);
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
    c.useTLS         = m_tlsGroup->isChecked();
    c.caCertPath     = m_caCertEdit->text().trimmed();
    c.clientCertPath = m_clientCertEdit->text().trimmed();
    c.clientKeyPath  = m_clientKeyEdit->text().trimmed();
    return c;
}

void ConnectionDialog::generateClientId()
{
    QString uuid = QUuid::createUuid().toString();
    // Strip braces
    uuid.remove('{').remove('}');
    m_clientIdEdit->setText("mqtt_" + uuid.left(8));
}

void ConnectionDialog::browseCaCert()
{
    QString path = QFileDialog::getOpenFileName(this, "Select CA Certificate",
                                                QString(), "PEM Files (*.pem *.crt *.cer);;All Files (*)");
    if (!path.isEmpty())
        m_caCertEdit->setText(path);
}

void ConnectionDialog::browseClientCert()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Client Certificate",
                                                QString(), "PEM Files (*.pem *.crt *.cer);;All Files (*)");
    if (!path.isEmpty())
        m_clientCertEdit->setText(path);
}

void ConnectionDialog::browseClientKey()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Client Key",
                                                QString(), "PEM Files (*.pem *.key);;All Files (*)");
    if (!path.isEmpty())
        m_clientKeyEdit->setText(path);
}
