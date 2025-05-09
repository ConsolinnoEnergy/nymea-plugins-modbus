
#ifndef INTEGRATIONPLUGINCHINT_H
#define INTEGRATIONPLUGINCHINT_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "dtsu666modbusrtuconnection.h"

#include "extern-plugininfo.h"

#include <QObject>

class IntegrationPluginChint: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginchint.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginChint();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_refreshTimer = nullptr;
    QHash<Thing *, DTSU666ModbusRtuConnection *> m_dtsu666Connections;
};

#endif // INTEGRATIONPLUGINCHINT_H
