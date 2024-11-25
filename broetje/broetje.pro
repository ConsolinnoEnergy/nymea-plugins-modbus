include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginbroetje.h \
    discoveryrtu.h

SOURCES += \
    integrationpluginbroetje.cpp \
    discoveryrtu.cpp

