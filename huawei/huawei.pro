include(../plugins.pri)

# Generate modbus connection
#MODBUS_TOOLS_CONFIG += VERBOSE
MODBUS_CONNECTIONS += huawei-fusion-solar-registers.json huawei-registers.json
include(../modbus.pri)

HEADERS += \
    huaweifusionsolar.h \
    huaweifusionsolardiscovery.h \
    discoveryrtu.h \
    integrationpluginhuawei.h

SOURCES += \
    huaweifusionsolar.cpp \
    huaweifusionsolardiscovery.cpp \
    discoveryrtu.cpp \
    integrationpluginhuawei.cpp
