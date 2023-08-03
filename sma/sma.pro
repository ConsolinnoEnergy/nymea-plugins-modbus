include(../plugins.pri)

QT += network

# Generate modbus connection
MODBUS_CONNECTIONS += sma-inverter-registers.json
MODBUS_TOOLS_CONFIG += VERBOSE
include(../modbus.pri)

SOURCES += \
    integrationpluginsma.cpp \
    modbus/smamodbusdiscovery.cpp \
    speedwire/speedwirediscovery.cpp \
    speedwire/speedwireinterface.cpp \
    speedwire/speedwireinverter.cpp \
    speedwire/speedwireinverterreply.cpp \
    speedwire/speedwireinverterrequest.cpp \
    speedwire/speedwiremeter.cpp \
    sunnywebbox/sunnywebbox.cpp \
    sunnywebbox/sunnywebboxdiscovery.cpp

HEADERS += \
    integrationpluginsma.h \
    modbus/smamodbusdiscovery.h \
    sma.h \
    speedwire/speedwire.h \
    speedwire/speedwirediscovery.h \
    speedwire/speedwireinterface.h \
    speedwire/speedwireinverter.h \
    speedwire/speedwireinverterreply.h \
    speedwire/speedwireinverterrequest.h \
    speedwire/speedwiremeter.h \
    sunnywebbox/sunnywebbox.h \
    sunnywebbox/sunnywebboxdiscovery.h
