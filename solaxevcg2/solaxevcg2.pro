include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solaxevcg2-registers.json
include(../modbus.pri)

HEADERS += \
    discoverytcp.h \
    integrationpluginsolaxevcg2.h \

SOURCES += \
    discoverytcp.cpp \
    integrationpluginsolaxevcg2.cpp \
