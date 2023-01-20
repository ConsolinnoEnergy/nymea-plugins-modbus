include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += kaco-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginkaco.h

SOURCES += \
    integrationpluginkaco.cpp
