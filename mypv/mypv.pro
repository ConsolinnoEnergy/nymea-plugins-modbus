include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += acelwa-registers.json

include(../modbus.pri)

SOURCES += \
    integrationpluginmypv.cpp \
    mypvmodbustcpconnection.cpp

HEADERS += \
    integrationpluginmypv.h \
    mypvmodbustcpconnection.h
