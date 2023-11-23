include(../plugins.pri)
MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += abb-registers.json

MODBUS_TOOLS_CONFIG += VERBOSE

include(../modbus.pri)

HEADERS += \
    terraRTUdiscovery.h \
    terraTCPdiscovery.h \
    integrationpluginabb.h

SOURCES += \
    terraRTUdiscovery.cpp \
    terraTCPdiscovery.cpp \
    integrationpluginabb.cpp
