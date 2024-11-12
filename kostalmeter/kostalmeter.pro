include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += sdm630-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoveryrtu.h \
    integrationpluginkostalmeter.h

SOURCES += \
    discoveryrtu.cpp \
    integrationpluginkostalmeter.cpp

