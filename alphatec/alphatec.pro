include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)
MODBUS_CONNECTIONS += alphatec-registers.json
include(../modbus.pri)
SOURCES += \
    integrationpluginalphatec.cpp

HEADERS += \
    integrationpluginalphatec.h

