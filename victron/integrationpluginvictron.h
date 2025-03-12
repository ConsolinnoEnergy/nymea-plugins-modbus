/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2022, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
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
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINVICTRON_H
#define INTEGRATIONPLUGINVICTRON_H

#include <plugintimer.h>
#include <integrations/integrationplugin.h>
#include <network/networkdevicemonitor.h>

#include "extern-plugininfo.h"

#include "victronsystemmodbustcpconnection.h"
#include "victronvebusmodbustcpconnection.h"
#include "victrongridmodbustcpconnection.h"

class IntegrationPluginVictron: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginvictron.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginVictron();

    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    const int m_modbusTcpPort = 502;
    const quint16 m_systemModbusSlaveAddress = 100;
    const quint16 m_vebusModbusSlaveAddress = 227;
    PluginTimer *m_refreshTimer = nullptr;
    quint8 m_setpointTimer = 0;

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, VictronSystemModbusTcpConnection *> m_systemTcpConnections;
    QHash<Thing *, VictronVebusModbusTcpConnection *> m_vebusTcpConnections;
    QHash<Thing *, VictronGridModbusTcpConnection *> m_gridTcpConnections;

    void setupVictronConnection(ThingSetupInfo *info);
    void activateRemoteControl(Thing *thing, bool activation);

    Thing *getMeterThing(Thing *parentThing);
    Thing *getBatteryThing(Thing *parentThing);
};

#endif // INTEGRATIONPLUGINVICTRON_H


