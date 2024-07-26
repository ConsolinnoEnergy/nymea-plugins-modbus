include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += foxess-v3-registers.json
MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginfoxesswbv3.cpp \
    foxesswbdiscovery.cpp \

HEADERS += \
    integrationpluginfoxesswbv3.h \
    foxesswbdicovery.h \
