include(../plugins.pri)

MODBUS_CONNECTIONS += alfen-registers.json
include(../modbus.pri)

SOURCES += \
    integrationpluginalfen.cpp \
    alfenwallboxmodbustcpconnection.cpp

HEADERS += \
    integrationpluginalfen.h \
    alfenwallboxmodbustcpconnection.h
