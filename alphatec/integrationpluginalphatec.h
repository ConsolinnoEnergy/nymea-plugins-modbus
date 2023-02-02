/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Tim Stöcker <t.stoecker@consolinno.de>                 *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation;                  *
 *  version 3 of the License.                                              *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINALPHATEC_H
#define INTEGRATIONPLUGINALPHATEC_H

#include "integrations/integrationplugin.h"
#include "alphatecwallboxmodbusrtuconnection.h"
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <QObject>
#include <QHostAddress>
#include <QTimer>
#include <plugintimer.h>

class NetworkDeviceMonitor;

class IntegrationPluginAlphatec: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginalphatec.json")
    Q_INTERFACES(IntegrationPlugin)


public:
    explicit IntegrationPluginAlphatec();

    void init() override;

    void discoverThings(ThingDiscoveryInfo *info) override;

    void setupThing(ThingSetupInfo *info) override;

    void postSetupThing(Thing *thing) override;

    void executeAction(ThingActionInfo *info) override;

    void thingRemoved(Thing *thing) override;

private:
    QHash<Thing *, AlphatecWallboxModbusRtuConnection *> m_rtuConnections;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    PluginTimer *m_pluginTimer = nullptr;

};

#endif // INTEGRATIONPLUGINALPHATEC_H
