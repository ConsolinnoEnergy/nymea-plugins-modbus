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

#ifndef INTEGRATIONPLUGINGOODWE_H
#define INTEGRATIONPLUGINGOODWE_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "goodwemodbusrtuconnection.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

class NetworkDeviceMonitor;

class IntegrationPluginGoodwe: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugingoodwe.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginGoodwe();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_pluginTimer = nullptr;

    struct MeterStates {
        bool modbusReachable {false};
        bool meterCommStatus {false};
    };

    struct BatteryStates {
        bool modbusReachable {false};
        quint32 batteryPower {0};
        GoodweModbusRtuConnection::BatteryStatus mode {GoodweModbusRtuConnection::BatteryStatusNoBattery};
    };

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, GoodweModbusRtuConnection *> m_rtuConnections;
    QHash<Thing *, MeterStates> m_meterstates;
    QHash<Thing *, BatteryStates> m_batterystates;

    void setWorkMode(Thing *thing, GoodweModbusRtuConnection::WorkMode state);
    void setBatteryStates(Thing *thing, GoodweModbusRtuConnection::BatteryStatus mode);
};

#endif // INTEGRATIONPLUGINGOODWE_H

