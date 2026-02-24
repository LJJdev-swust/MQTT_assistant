#include "src/ui/mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MQTT Assistant");
    app.setOrganizationName("MQTTAssistant");

    // Ensure app data directory exists
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // Load stylesheet
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    MainWindow w;
    w.show();
    return app.exec();
}
