include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solax-ultra-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsolax-ultra.h \
    solaxmodbustcpconnection.h \
    discoverytcp.h

SOURCES += \
    integrationpluginsolax-ultra.cpp \
    solaxmodbustcpconnection.cpp \
    discoverytcp.cpp
