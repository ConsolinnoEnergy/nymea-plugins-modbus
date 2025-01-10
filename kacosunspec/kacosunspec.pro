include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += kaco-sunspec-registers.json kaco-nh3-registers.json
include(../modbus.pri)

HEADERS += \
    integrationpluginkacosunspec.h \
    kaconh3discovery.h

SOURCES += \
    integrationpluginkacosunspec.cpp \
    kaconh3discovery.cpp
