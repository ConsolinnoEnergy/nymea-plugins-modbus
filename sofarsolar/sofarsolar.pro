include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += sofarsolar-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsofarsolar.h

SOURCES += \
    integrationpluginsofarsolar.cpp