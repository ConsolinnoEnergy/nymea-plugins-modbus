include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += umg604-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoverytcp.h \
    integrationpluginjanitza.h

SOURCES += \
    discoverytcp.cpp \
    integrationpluginjanitza.cpp

