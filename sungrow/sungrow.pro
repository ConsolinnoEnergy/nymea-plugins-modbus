include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += sungrow-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginsungrow.h \
    sungrowdiscovery.h \
    sungrowmodbustcpconnection.h

SOURCES += \
    integrationpluginsungrow.cpp \
    sungrowdiscovery.cpp \
    sungrowmodbustcpconnection.cpp
