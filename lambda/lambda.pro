include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += lambda-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginlambda.cpp

HEADERS += \
    integrationpluginlambda.h
