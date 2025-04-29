/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
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

#ifndef INTEGRATIONPLUGINSIEMENS_H
#define INTEGRATIONPLUGINSIEMENS_H

#include <integrations/integrationplugin.h>
#include "extern-plugininfo.h"

#include "siemenspac2200modbustcpconnection.h"
#include "siemenspac2200.h"

#include <QObject>
#include <QHostAddress>

class NetworkDeviceMonitor;
class PluginTimer;

class IntegrationPluginSiemens : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsiemens.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginSiemens();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    // QHash<Thing*, SiemensPAC2200*> m_siemensDevices;
    QHash<Thing*, NetworkDeviceMonitor*> m_monitors;
    PluginTimer *m_pluginTimer = nullptr;

    void updateMeasurements(Thing *thing);
    void setDeviceStatus(Thing *thing, quint32 status);
    void setVoltagePhaseA(Thing *thing, double voltage);
    void setCurrentPhaseA(Thing *thing, double current);
    void setCurrentPower(Thing *thing, double power);
    void setTotalEnergy(Thing *thing, double energy);
    void setFrequency(Thing *thing, double frequency);
    void handleConnectionError(Thing *thing);
};

#endif // INTEGRATIONPLUGINSIEMENS_H