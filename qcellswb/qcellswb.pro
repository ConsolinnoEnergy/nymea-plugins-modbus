include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += qcells-registers.json
# MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginqcellswb.cpp \
    qcellsmodbustcpconnection.cpp

HEADERS += \
    integrationpluginqcellswb.h \
    qcellsmodbustcpconnection.h
