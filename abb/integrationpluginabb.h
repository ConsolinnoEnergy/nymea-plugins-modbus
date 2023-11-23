/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
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
#ifndef INTEGRATIONPLUGINABB_H
#define INTEGRATIONPLUGINABB_H

#include <plugintimer.h>
#include <integrations/integrationplugin.h>
#include <network/networkdevicemonitor.h>

#include "extern-plugininfo.h"

#include "abbmodbusrtuconnection.h"
#include "abbmodbustcpconnection.h"

#define MIN_FIRMWARE_VERSION_MAJOR      0x00
#define MIN_FIRMWARE_VERSION_MINOR      0x00
#define MIN_FIRMWARE_VERSION_REVISION   0x00

#define MIN_FIRMWARE_VERSION ((MIN_FIRMWARE_VERSION_MAJOR << 16) | (MIN_FIRMWARE_VERSION_MINOR << 8) | MIN_FIRMWARE_VERSION_REVISION)


class IntegrationPluginABB: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginabb.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginABB();

    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private:
    void setupRtuConnection(ThingSetupInfo *info);
    void setupTcpConnection(ThingSetupInfo *info);

private:
    PluginTimer *m_pluginTimer = nullptr;
    QHash<Thing *, ABBModbusRtuConnection*> m_rtuConnections;
    QHash<Thing *, ABBModbusTcpConnection*> m_tcpConnections;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;

};

#endif // INTEGRATIONPLUGINABB_H


