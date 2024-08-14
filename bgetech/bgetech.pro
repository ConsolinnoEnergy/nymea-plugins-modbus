include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += sdm630-registers.json sdm72-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoveryrtu.h \
    integrationpluginbgetech.h

SOURCES += \
    discoveryrtu.cpp \
    integrationpluginbgetech.cpp

