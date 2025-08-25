include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solax-ultra-registers.json x3-ies-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsolaxonly.h \
    solaxmodbustcpconnection.h \
    solaxiesmodbustcpconnection.h \
    discoverytcp.h \
    discoveryIES.h

SOURCES += \
    integrationpluginsolaxonly.cpp \
    solaxmodbustcpconnection.cpp \
    solaxiesmodbustcpconnection.cpp \
    discoverytcp.cpp \
    discoveryIES.cpp
