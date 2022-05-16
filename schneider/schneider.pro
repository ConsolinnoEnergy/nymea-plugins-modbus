include(../plugins.pri)

QT += \ 
    network \
    serialbus \

SOURCES += \
    integrationpluginschneider.cpp \
    schneiderwallbox.cpp \
    schneidermodbustcpconnection.cpp \
    ../modbus/modbustcpmaster.cpp \
    ../modbus/modbusdatautils.cpp

HEADERS += \
    integrationpluginschneider.h \
    schneiderwallbox.h \
    schneidermodbustcpconnection.h \
    ../modbus/modbustcpmaster.h \
    ../modbus/modbusdatautils.h
