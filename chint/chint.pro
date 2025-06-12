include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += dtsu666-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoveryrtu.h \
    integrationpluginchint.h

SOURCES += \
    discoveryrtu.cpp \
    integrationpluginchint.cpp
