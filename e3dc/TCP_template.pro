include(../plugins.pri)

QT += network serialbus

SOURCES += \
    integrationpluginTCP_template.cpp \
    tcp_modbusconnection.cpp \
    ../modbus/modbustcpmaster.cpp \
    ../modbus/modbusdatautils.cpp

HEADERS += \
    integrationpluginTCP_template.h \
    tcp_modbusconnection.h \
    ../modbus/modbustcpmaster.h \
    ../modbus/modbusdatautils.h
