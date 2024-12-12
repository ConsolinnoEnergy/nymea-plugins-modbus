include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += umg604-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    discoveryrtu.h \
    integrationpluginjanitza.h

SOURCES += \
    discoveryrtu.cpp \
    integrationpluginjanitza.cpp

