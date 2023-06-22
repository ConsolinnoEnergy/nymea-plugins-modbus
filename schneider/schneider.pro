include(../plugins.pri)

MODBUS_CONNECTIONS += schneiderwallbox-registers.json
include(../modbus.pri)

SOURCES += \
    integrationpluginschneider.cpp \
    schneiderwallbox.cpp

HEADERS += \
    integrationpluginschneider.h \
    schneiderwallbox.h
