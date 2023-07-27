/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Felix Stoecker <f.stoecker@consolinno.de>                 *
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

#ifndef INTEGRATIONPLUGINSAX_H
#define INTEGRATIONPLUGINSAX_H

#include "integrations/integrationplugin.h"
#include "extern-plugininfo.h"

#include <QObject>

class SaxModbusTcpConnection;
class NetworkDeviceMonitor;
class PluginTimer;

class IntegrationPluginSax: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsax.json")
    Q_INTERFACES(IntegrationPlugin)


public:
    explicit IntegrationPluginSax();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    QHash<Thing*, SaxModbusTcpConnection*> m_tcpConnections;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    PluginTimer *m_pluginTimer = nullptr;  
};

#endif // INTEGRATIONPLUGINSAX_H
