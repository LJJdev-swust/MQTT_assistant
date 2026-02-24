#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include "core/models.h"

class ConnectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);
    explicit ConnectionDialog(const MqttConnectionConfig &config, QWidget *parent = nullptr);

    MqttConnectionConfig config() const;

private slots:
    void generateClientId();
    void browseCaCert();
    void browseClientCert();
    void browseClientKey();

private:
    void setupUi();
    void populateFrom(const MqttConnectionConfig &config);

    QLineEdit  *m_nameEdit;
    QLineEdit  *m_hostEdit;
    QSpinBox   *m_portSpin;
    QLineEdit  *m_usernameEdit;
    QLineEdit  *m_passwordEdit;
    QLineEdit  *m_clientIdEdit;
    QPushButton *m_generateBtn;
    QCheckBox  *m_cleanSessionCheck;
    QSpinBox   *m_keepAliveSpin;

    QGroupBox  *m_tlsGroup;
    QLineEdit  *m_caCertEdit;
    QPushButton *m_caCertBtn;
    QLineEdit  *m_clientCertEdit;
    QPushButton *m_clientCertBtn;
    QLineEdit  *m_clientKeyEdit;
    QPushButton *m_clientKeyBtn;
};

#endif // CONNECTIONDIALOG_H
