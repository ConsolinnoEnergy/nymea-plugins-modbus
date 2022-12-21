include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)
MODBUS_CONNECTIONS += alphatec-registers.json
include(../modbus.pri)
SOURCES += \
    alphatecWallbox.cpp \
    integrationpluginalphatec.cpp

HEADERS += \
    alphatecWallbox.h \
    integrationpluginalphatec.h

