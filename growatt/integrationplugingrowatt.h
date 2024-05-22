/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2024, Consolinno Energy GmbH
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

#ifndef INTEGRATIONPLUGINGROWATT_H
#define INTEGRATIONPLUGINGROWATT_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "growattmodbusrtuconnection.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

//#include <hardware/modbus/modbusrtuconnection.h>



class IntegrationPluginGrowatt : public IntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "nymea.IntegrationPlugin" FILE "integrationplugingrowatt.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginGrowatt();

    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void executeAction(ThingActionInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    //void setupGrowattConnection(ThingSetupInfo *info);

    QHash<Thing *, GrowattModbusRtuConnection *> m_rtuConnections;
    //QHash<Thing *, SerialPort *> m_serialPorts;
    PluginTimer *m_pluginTimer = nullptr;
};

#endif // INTEGRATIONPLUGINGROWATT_H
