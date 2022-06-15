include(../plugins.pri)

QT += network serialbus

SOURCES += \
    e3dcbattery.cpp \
    e3dcinverter.cpp \
    e3dcwallbox.cpp \
    integrationplugine3dc.cpp \
    tcp_modbusconnection.cpp \
    ../modbus/modbustcpmaster.cpp \
    ../modbus/modbusdatautils.cpp

HEADERS += \
    e3dcbattery.h \
    e3dcinverter.h \
    e3dcwallbox.h \
    integrationplugine3dc.h \
    tcp_modbusconnection.h \
    ../modbus/modbustcpmaster.h \
    ../modbus/modbusdatautils.h
