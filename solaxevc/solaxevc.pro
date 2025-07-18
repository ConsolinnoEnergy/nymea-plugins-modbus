include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += x3wallbox-registers.json evc-standalone-registers.json
include(../modbus.pri)

HEADERS += \
    discoverytcp.h \
    discoverystandalone.h \
    integrationpluginsolaxevc.h \
    solaxevcmodbustcpconnection.h \
    solaxevcstandalonemodbustcpconnection.h \

SOURCES += \
    discoverytcp.cpp \
    discoverystandalone.cpp \
    integrationpluginsolaxevc.cpp \
    solaxevcmodbustcpconnection.cpp \
    solaxevcstandalonemodbustcpconnection.cpp
