include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solax-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsolax.h \
    solaxmodbustcpconnection.h \
    discoverytcp.h \
    discoveryrtu.h

SOURCES += \
    integrationpluginsolax.cpp \
    solaxmodbustcpconnection.cpp \
    discoverytcp.cpp \
    discoveryrtu.cpp
