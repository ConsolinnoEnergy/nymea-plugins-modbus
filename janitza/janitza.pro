include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += umg604-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoveryrtu.h \
    discoverytcp.h \
    integrationpluginjanitza.h

SOURCES += \
    discoveryrtu.cpp \
    discoverytcp.cpp \
    integrationpluginjanitza.cpp

