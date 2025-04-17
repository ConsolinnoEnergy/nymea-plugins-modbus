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

#ifndef INTEGRATIONPLUGINSOFARSOLAR_H
#define INTEGRATIONPLUGINSOFARSOLAR_H

#include <integrations/integrationplugin.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <plugintimer.h>

#include "extern-plugininfo.h"
#include "sofarsolarmodbusrtuconnection.h"
#include "powercontrolsofarsolar.h"

#include <QObject>
#include <QTimer>

class IntegrationPluginSofarsolar : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsofarsolar.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSofarsolar();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private:
    bool isOutlier(const QList<float> &list);
    PluginTimer *m_pluginTimer = nullptr;
    int m_windowLength{7};
    PowerControlSofarsolar *m_powerControl;

    void executeExportLimitAction(ThingActionInfo *info, bool &retFlag);
    bool exportPowerControl(SofarsolarModbusRtuConnection *sofarsolarmodbusrtuconnection, quint32 value);
    bool handleReply(ModbusRtuReply *reply);

    QHash<Thing *, QList<float>> m_pvEnergyProducedValues;
    QHash<Thing *, QList<float>> m_energyConsumedValues;
    QHash<Thing *, QList<float>> m_energyProducedValues;

    struct PvPower
    {
        quint16 power1{0};
        quint16 power2{0};
    };

    QHash<Thing *, SofarsolarModbusRtuConnection *> m_rtuConnections;
    QHash<Thing *, PvPower> m_pvpower;

    void setSystemStatus(Thing *thing, SofarsolarModbusRtuConnection::SystemStatus state);
};

#endif // INTEGRATIONPLUGINSOFARSOLAR_H
