/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
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

#ifndef INTEGRATIONPLUGINSOLAX_H
#define INTEGRATIONPLUGINSOLAX_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "solaxmodbusrtuconnection.h"
#include "solaxmodbustcpconnection.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

class NetworkDeviceMonitor;

class IntegrationPluginSolax: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsolax.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSolax();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    PluginTimer *m_pluginTimer = nullptr;
    bool m_setupTcpConnectionRunning = false;

    struct MeterStates {
        bool modbusReachable {false};
        bool meterCommStatus {true};
    };

    struct BatteryStates {
        bool modbusReachable {false};
        bool bmsCommStatus {false};
        quint16 bmsWarningLsb {0};
        quint16 bmsWarningMsb {0};
    };

    QHash<Thing *, MeterStates> m_meterstates;
    QHash<Thing *, BatteryStates> m_batterystates;

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, SolaxModbusTcpConnection *> m_tcpConnections;
    QHash<Thing *, SolaxModbusRtuConnection *> m_rtuConnections;
    
    QTimer *m_batteryPowerTimer = nullptr;

    void setRunMode(Thing *thing, quint16 runModeAsInt);
    void setErrorMessage(Thing *thing, quint32 errorBits);
    void setBmsWarningMessage(Thing *thing);

    void setupTcpConnection(ThingSetupInfo *info);
};

#endif // INTEGRATIONPLUGINSOLAX_H

