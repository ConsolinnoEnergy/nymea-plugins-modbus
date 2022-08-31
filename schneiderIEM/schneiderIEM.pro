include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += schneiderIEM-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginschneiderIEM.h
    schneideriemmodbusrtuconnection.h

SOURCES += \
    integrationpluginschneiderIEM.cpp
    schneideriemmodbusrtuconnection.cpp

