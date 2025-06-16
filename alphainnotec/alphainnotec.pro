include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += alphainnotec-registers.json ait-shi-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginalphainnotec.cpp \
    aitdiscovery.cpp

HEADERS += \
    integrationpluginalphainnotec.h \
    aitdiscovery.h
