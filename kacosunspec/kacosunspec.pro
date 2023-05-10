include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += kaco-sunspec-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginkacosunspec.h

SOURCES += \
    integrationpluginkacosunspec.cpp
