include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += azzurro-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginazzurro.h \
    powercontrolazzurro.h

SOURCES += \
    integrationpluginazzurro.cpp \
    powercontrolazzurro.cpp
