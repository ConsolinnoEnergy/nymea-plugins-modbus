include(../plugins.pri)

MODBUS_CONNECTIONS += alfen-registers.json
include(../modbus.pri)

SOURCES += \
    integrationpluginalfen.cpp \

HEADERS += \
    integrationpluginalfen.h \
