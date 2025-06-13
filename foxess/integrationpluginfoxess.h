/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2025, Consolinno Energy GmbH
* Contact: info@consolinno.de
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINFOXESS_H
#define INTEGRATIONPLUGINFOXESS_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>
#include <network/networkdevicemonitor.h>

#include "network/zeroconf/zeroconfservicebrowser.h"
#include "network/zeroconf/zeroconfserviceentry.h"

#include "extern-plugininfo.h"
#include "rseriesmodbusrtuconnection.h"
#include "foxessmodbustcpconnection.h"

#include <QObject>
#include <QTimer>

class IntegrationPluginFoxESS: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginfoxess.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginFoxESS();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_pluginTimer = nullptr;

    // R-Series Inverter
    QHash<Thing *, RSeriesModbusRtuConnection*> m_rtuConnections;
    void setOperatingState(Thing *thing, RSeriesModbusRtuConnection::OperatingState state);

    // A-Series Wallbox
    QTimer *m_chargeLimitTimer = nullptr;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    ZeroConfServiceBrowser *m_serviceBrowser = nullptr;
    QHash<Thing *, FoxESSModbusTcpConnection *> m_tcpConnections;
    bool m_setupTcpConnectionRunning = false;

    void setupTcpConnection(ThingSetupInfo *info);
    void toggleCharging(FoxESSModbusTcpConnection *connection, bool power);
    void setMaxCurrent(FoxESSModbusTcpConnection *connection, float maxCurrent, int phaseCount);
};

#endif // INTEGRATIONPLUGINFOXESS_H

