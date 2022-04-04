include(../plugins.pri)

QT += network serialbus

SOURCES += \
    integrationplugine3dc.cpp \
    tcp_modbusconnection.cpp \
    ../modbus/modbustcpmaster.cpp \
    ../modbus/modbusdatautils.cpp

HEADERS += \
    integrationplugine3dc.h \
    tcp_modbusconnection.h \
    ../modbus/modbustcpmaster.h \
    ../modbus/modbusdatautils.h
