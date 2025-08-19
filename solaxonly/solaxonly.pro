include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solax-ultra-registers.json x3-ies-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsolaxonly.h \
    solaxmodbustcpconnection.h \
    discoverytcp.h

SOURCES += \
    integrationpluginsolaxonly.cpp \
    solaxmodbustcpconnection.cpp \
    discoverytcp.cpp
