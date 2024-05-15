include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += dvmodbusir-registers.json
include(../modbus.pri)

SOURCES += \
    integrationplugindvmodbusir.cpp

HEADERS += \
    integrationplugindvmodbusir.h
