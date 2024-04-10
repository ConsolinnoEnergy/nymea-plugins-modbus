include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solaxevc-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsolaxevc.h

SOURCES += \
    integrationpluginsolaxevc.cpp
