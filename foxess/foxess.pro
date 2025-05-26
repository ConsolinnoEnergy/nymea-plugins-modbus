include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += rseries-registers.json
include(../modbus.pri)

HEADERS += \
    rseriesdiscovery.h \
    integrationpluginfoxess.h

SOURCES += \
    rseriesdiscovery.cpp \
    integrationpluginfoxess.cpp
