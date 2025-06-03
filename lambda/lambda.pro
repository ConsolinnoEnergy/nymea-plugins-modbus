include(../plugins.pri)

PKGCONFIG += nymea-gpio

# Generate modbus connection
MODBUS_CONNECTIONS += lambda-registers.json
#MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginlambda.cpp \
    lambdamodbustcpconnection.cpp \
    lpcinterface.cpp

HEADERS += \
    integrationpluginlambda.h \
    lambdamodbustcpconnection.h \
    lpcinterface.h
