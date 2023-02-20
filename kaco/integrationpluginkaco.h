/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2023 Sebastian Ringer <s.ringer@consolinno.de>           *
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

#ifndef INTEGRATIONPLUGINKACO_H
#define INTEGRATIONPLUGINKACO_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "kacomodbustcpconnection.h"
#include "kacomodbusrtuconnection.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

class NetworkDeviceMonitor;

class IntegrationPluginKaco: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginkaco.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginKaco();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

private:
    PluginTimer *m_pluginTimer = nullptr;

    struct ScaleFactors {
        qint16 powerSf {1};
        qint16 energySf {1};
    };

    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, KacoModbusTcpConnection *> m_tcpConnections;
    QHash<Thing *, KacoModbusRtuConnection *> m_rtuConnections;
    QHash<Thing *, ScaleFactors> m_scalefactors;

    void setOperatingState(Thing *thing, KacoModbusTcpConnection::OperatingState state);
    void setOperatingState(Thing *thing, KacoModbusRtuConnection::OperatingState state);
};

#endif // INTEGRATIONPLUGINKACO_H

