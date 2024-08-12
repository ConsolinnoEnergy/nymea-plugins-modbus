include(../plugins.pri)

MODBUS_CONNECTIONS += amperfied-registers-rtu.json amperfied-registers-tcp.json

MODBUS_TOOLS_CONFIG += VERBOSE

include(../modbus.pri)

HEADERS += \
    energycontroldiscovery.h \
    connecthomediscovery.h \
    integrationpluginamperfied.h

SOURCES += \
    energycontroldiscovery.cpp \
    connecthomediscovery.cpp \
    integrationpluginamperfied.cpp
