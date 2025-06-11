include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += \
    x3inverter-registers.json \
    solaxevcg2-registers.json

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
