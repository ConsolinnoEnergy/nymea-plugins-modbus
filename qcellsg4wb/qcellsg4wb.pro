include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += qcells-registers.json
# MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginqcellsg4wb.cpp \
    qcellsmodbustcpconnection.cpp

HEADERS += \
    integrationpluginqcellsg4wb.h \
    qcellsmodbustcpconnection.h
