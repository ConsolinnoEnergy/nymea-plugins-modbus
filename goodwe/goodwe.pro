include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += goodwe-registers.json
include(../modbus.pri)

HEADERS += \
    integrationplugingoodwe.h

SOURCES += \
    integrationplugingoodwe.cpp
