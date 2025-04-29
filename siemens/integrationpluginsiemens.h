#ifndef INTEGRATIONPLUGINSIEMENS_H
#define INTEGRATIONPLUGINSIEMENS_H

#include <integrations/integrationplugin.h>
#include "extern-plugininfo.h"

#include <QObject>
#include <QHostAddress>

class NetworkDeviceMonitor;
class PluginTimer;

class IntegrationPluginSiemens : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsiemens.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSiemens();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    QHash<Thing*, NetworkDeviceMonitor*> m_monitors;
    PluginTimer *m_pluginTimer = nullptr;

    void updateMeasurements(Thing *thing);
    void setDeviceStatus(Thing *thing, quint32 status);
    void setVoltagePhaseA(Thing *thing, double voltage);
    void setCurrentPhaseA(Thing *thing, double current);
    void setCurrentPower(Thing *thing, double power);
    void setTotalEnergy(Thing *thing, double energy);
    void setFrequency(Thing *thing, double frequency);
    void handleConnectionError(Thing *thing);
};

#endif // INTEGRATIONPLUGINSIEMENS_H