include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += mypv-registers.json

include(../modbus.pri)

SOURCES += \
    integrationpluginmypv.cpp

HEADERS += \
    integrationpluginmypv.h
