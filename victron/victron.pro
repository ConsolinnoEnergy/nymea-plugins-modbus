include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += victronsystem-registers.json victronvebus-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginvictron.h \
    victrondiscovery.h \
    victronvebusmodbustcpconnection.h

SOURCES += \
    integrationpluginvictron.cpp \
    victrondiscovery.cpp \
    victronvebusmodbustcpconnection.cpp
