QT += core gui widgets sql network mqtt
DEFINES += QT_SSL_USE_OPENSSL
TARGET = MQTT_assistant
TEMPLATE = app
CONFIG += c++17

SOURCES += \
    main.cpp \
    src/core/mqttclient.cpp \
    src/core/databasemanager.cpp \
    src/core/scriptengine.cpp \
    src/ui/mainwindow.cpp \
    src/ui/dialogs/connectiondialog.cpp \
    src/ui/dialogs/commanddialog.cpp \
    src/ui/dialogs/scriptdialog.cpp \
    src/ui/widgets/chatwidget.cpp \
    src/ui/widgets/collapsiblesection.cpp \
    src/ui/widgets/messagebubbleitem.cpp \
    src/ui/widgets/connectionpanel.cpp \
    src/ui/widgets/commandpanel.cpp \
    src/ui/widgets/subscriptionpanel.cpp

HEADERS += \
    src/core/models.h \
    src/core/mqttclient.h \
    src/core/databasemanager.h \
    src/core/scriptengine.h \
    src/ui/mainwindow.h \
    src/ui/dialogs/connectiondialog.h \
    src/ui/dialogs/commanddialog.h \
    src/ui/dialogs/scriptdialog.h \
    src/ui/widgets/chatwidget.h \
    src/ui/widgets/collapsiblesection.h \
    src/ui/widgets/messagebubbleitem.h \
    src/ui/widgets/connectionpanel.h \
    src/ui/widgets/commandpanel.h \
    src/ui/widgets/subscriptionpanel.h


RESOURCES += resources/resources.qrc

RC_ICONS = MQTT.ico

INCLUDEPATH += src
