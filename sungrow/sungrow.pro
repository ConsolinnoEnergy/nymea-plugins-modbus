include(../plugins.pri)

# Generate modbus connection
MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += sungrow-registers-rtu.json sungrow-registers-tcp.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsungrow.h

SOURCES += \
    integrationpluginsungrow.cpp
