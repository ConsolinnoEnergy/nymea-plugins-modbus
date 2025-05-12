#ifndef INTEGRATIONPLUGINSIEMENS_H
#define INTEGRATIONPLUGINSIEMENS_H

#include <integrations/integrationplugin.h>
#include <network/networkdevicemonitor.h>
#include <plugintimer.h>

#include "siemenspac2200modbustcpconnection.h"
#include "extern-plugininfo.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>


class IntegrationPluginSiemens : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsiemens.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSiemens();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    bool m_setupTcpConnectionRunning = false;

    PluginTimer *m_pluginTimer = nullptr;

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, SiemensPAC2200ModbusTcpConnection *> m_pac220TcpConnections;
};

#endif // INTEGRATIONPLUGINSIEMENS_H
