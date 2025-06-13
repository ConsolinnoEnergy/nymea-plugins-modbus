include(../plugins.pri)

# Generate modbus connection
# MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += rseries-registers.json foxess-v3-registers.json
include(../modbus.pri)

HEADERS += \
    rseriesdiscovery.h \
    foxessmodbustcpconnection.h \
    integrationpluginfoxess.h \

SOURCES += \
    rseriesdiscovery.cpp \
    foxessmodbustcpconnection.cpp \
    integrationpluginfoxess.cpp \
