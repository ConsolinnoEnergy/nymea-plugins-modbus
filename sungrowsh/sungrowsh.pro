include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += sungrow-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

HEADERS += \
    integrationpluginsungrowsh.h \
    sungrowdiscovery.h \
    sungrowmodbustcpconnection.h

SOURCES += \
    integrationpluginsungrowsh.cpp \
    sungrowdiscovery.cpp \
    sungrowmodbustcpconnection.cpp
