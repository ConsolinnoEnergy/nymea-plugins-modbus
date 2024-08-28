include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += qcellswb-registers.json
MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginqcellswb.cpp

HEADERS += \
    integrationpluginqcellswb.h
