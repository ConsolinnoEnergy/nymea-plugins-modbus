include(../plugins.pri)

# Generate modbus connection
MODBUS_CONNECTIONS += acelwa-registers.json

include(../modbus.pri)

SOURCES += \
    integrationpluginmypv-new.cpp \
    mypvmodbustcpconnection.cpp

HEADERS += \
    integrationpluginmypv-new.h \
    mypvmodbustcpconnection.h
