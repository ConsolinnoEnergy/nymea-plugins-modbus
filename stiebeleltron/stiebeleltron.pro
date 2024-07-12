include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += wpm-registers.json lwz-registers.json test-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginstiebeleltron.h \
    discovery.h

SOURCES += \
    integrationpluginstiebeleltron.cpp \
    discovery.cpp

