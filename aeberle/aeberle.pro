include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += aeberle-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginaeberle.h

SOURCES += \
    integrationpluginaeberle.cpp

