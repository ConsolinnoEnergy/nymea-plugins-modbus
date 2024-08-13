include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += solaxevc-registers.json
include(../modbus.pri)

HEADERS += \
    discoverytcp.h \
    integrationpluginsolaxevc.h \
    solaxevcmodbustcpconnection.h

SOURCES += \
    discoverytcp.cpp \
    integrationpluginsolaxevc.cpp \
    solaxevcmodbustcpconnection.cpp