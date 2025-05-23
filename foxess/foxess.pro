include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += rseries-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginfoxess.h \
    rseriesdiscvoery.h

SOURCES += \
    integrationpluginfoxess.cpp \
    rseriesdiscvoery.cpp
