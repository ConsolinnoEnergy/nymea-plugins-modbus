include(../plugins.pri)

MODBUS_CONNECTIONS += sax-registers.json
include(../modbus.pri)
SOURCES += \
    integrationpluginsax.cpp

HEADERS += \
    integrationpluginsax.h

