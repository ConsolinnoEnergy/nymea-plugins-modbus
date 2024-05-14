#ifndef INTEGRATIONPLUGINGROWATT_H
#define INTEGRATIONPLUGINGROWATT_H

#include "plugininfo.h"
#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuconnection.h>
#include <hardware/serialport.h>

class IntegrationPluginGrowatt : public IntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "nymea.IntegrationPlugin" FILE "integrationplugingrowatt.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginGrowatt();

    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    void setupGrowattConnection(ThingSetupInfo *info);

    QHash<Thing *, ModbusRtuConnection *> m_GrowattConnections;
    QHash<Thing *, SerialPort *> m_serialPorts;
    PluginTimer *m_pluginTimer = nullptr;
};

#endif // INTEGRATIONPLUGINGROWATT_H
