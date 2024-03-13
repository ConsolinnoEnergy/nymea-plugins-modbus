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

#ifndef INTEGRATIONPLUGINKACOSUNSPEC_H
#define INTEGRATIONPLUGINKACOSUNSPEC_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "kacosunspecmodbustcpconnection.h"
#include "kacosunspecmodbusrtuconnection.h"

#include <QObject>
#include <QHostAddress>
#include <QTimer>

class NetworkDeviceMonitor;

class IntegrationPluginKacoSunSpec: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginkacosunspec.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginKacoSunSpec();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

signals:
    void discoveryRtuFinished();

private:
    PluginTimer *m_pluginTimer = nullptr;

    struct ScaleFactors {
        qint16 powerSf {1};
        qint16 energySf {1};
    };

    quint16 m_openReplies;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;
    QHash<Thing *, KacoSunSpecModbusTcpConnection *> m_tcpConnections;
    QHash<Thing *, KacoSunSpecModbusRtuConnection *> m_rtuConnections;
    QHash<Thing *, ScaleFactors> m_scalefactors;

    void setOperatingState(Thing *thing, KacoSunSpecModbusTcpConnection::OperatingState state);
    void setOperatingState(Thing *thing, KacoSunSpecModbusRtuConnection::OperatingState state);
};

#endif // INTEGRATIONPLUGINKACOSUNSPEC_H

