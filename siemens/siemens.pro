include(../plugins.pri)

MODBUS_CONNECTIONS += pac2200-register.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsiemens.h \
    discoverytcp.h

SOURCES += \
    integrationpluginsiemens.cpp
    discoverytcp.cpp
