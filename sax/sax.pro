include(../plugins.pri)

MODBUS_CONNECTIONS += sax-registers.json
include(../modbus.pri)
SOURCES += \
    integrationpluginsax.cpp \
    saxstoragediscovery.cpp

HEADERS += \
    integrationpluginsax.h \
    saxstoragediscovery.h

