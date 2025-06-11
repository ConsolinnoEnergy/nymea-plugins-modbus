include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += \
    solaxevcg2-registers.json \
    x3inverter-registers.json

include(../modbus.pri)

HEADERS += \
    integrationpluginsolax.h \
    solaxmodbustcpconnection.h \
    discoverytcp.h \
    discoverytcpevcg2.h \
    discoveryrtu.h

SOURCES += \
    integrationpluginsolax.cpp \
    solaxmodbustcpconnection.cpp \
    discoverytcp.cpp \
    discoverytcpevcg2.cpp \
    discoveryrtu.cpp
