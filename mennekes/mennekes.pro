include(../plugins.pri)

QT += \ 
    network \
    serialbus \

SOURCES += \
    integrationpluginmennekes.cpp \
    mennekesmodbustcpconnection.cpp \
    mennekeswallbox.cpp \
    ../modbus/modbustcpmaster.cpp \
    ../modbus/modbusdatautils.cpp

HEADERS += \
    integrationpluginmennekes.h \
    mennekesmodbustcpconnection.h \
    mennekeswallbox.h \
    ../modbus/modbustcpmaster.h \
    ../modbus/modbusdatautils.h \
