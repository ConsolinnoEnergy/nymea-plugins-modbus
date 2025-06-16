include(../plugins.pri)

MODBUS_CONNECTIONS += pac2200-register.json
include(../modbus.pri)

HEADERS += \
    discoverytcp.h \
    integrationpluginsiemens.h

SOURCES += \
    discoverytcp.cpp \
    integrationpluginsiemens.cpp
