include(../plugins.pri)

MODBUS_CONNECTIONS += alfen-registers.json
include(../modbus.pri)

SOURCES += \
    integrationpluginalfen.cpp \
    alfen.cpp

HEADERS += \
    integrationpluginalfen.h \
    alfen.h
