include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += goodwe-registers.json
include(../modbus.pri)

HEADERS += \
    integrationplugingoodwe.h \
    goodwediscovery.h

SOURCES += \
    integrationplugingoodwe.cpp \
    goodwediscovery.cpp
