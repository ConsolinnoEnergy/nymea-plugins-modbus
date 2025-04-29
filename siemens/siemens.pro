include(../plugins.pri)

MODBUS_CONNECTIONS += pac-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginsiemens.h
    siemenspac2200.h

SOURCES += \
    integrationpluginsiemens.cpp
    siemenspac2200.cpp

