include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += sungrow-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsungrowsg.h \
    sungrowdiscovery.h

SOURCES += \
    integrationpluginsungrowsg.cpp \
    sungrowdiscovery.cpp
