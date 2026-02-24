QT += core gui widgets sql network mqtt
TARGET = MQTT_assistant
TEMPLATE = app
CONFIG += c++11

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
    src/ui/widgets/messagebubbleitem.cpp \
    src/ui/widgets/connectionpanel.cpp \
    src/ui/widgets/commandpanel.cpp

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
    src/ui/widgets/messagebubbleitem.h \
    src/ui/widgets/connectionpanel.h \
    src/ui/widgets/commandpanel.h

RESOURCES += resources/resources.qrc

INCLUDEPATH += src
