include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += alphatec-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginalphatec.h

SOURCES += \
    integrationpluginalphatec.cpp
