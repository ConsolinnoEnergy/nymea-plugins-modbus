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

#ifndef INTEGRATIONPLUGINALFEN_H
#define INTEGRATIONPLUGINALFEN_H

#include <integrations/integrationplugin.h>
#include "extern-plugininfo.h"

#include "alfenwallboxmodbustcpconnection.h"

#include <QObject>
#include <QHostAddress>

class NetworkDeviceMonitor;
class PluginTimer;

class IntegrationPluginAlfen : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginalfen.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginAlfen();
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    QHash<Thing*, NetworkDeviceMonitor*> m_monitors;
    PluginTimer *m_pluginTimer = nullptr;

    //AlfenWallboxModbusTcpConnection *alfenWallboxTcpConnection;
    //NetworkDeviceMonitor *monitor;
    QHash<Thing*, AlfenWallboxModbusTcpConnection *> m_modbusTcpConnections;
    bool setMaxCurrent(AlfenWallboxModbusTcpConnection *connection, int maxCurrent);
    int ampereValueBefore = 6;

    std::map<QString, QString> chargePointStates = {
        {"A", "A - EV not connected"},
        {"B1", "B1 - EVSE not ready"},
        {"B2", "B2 - EVSE ready"},
        {"C1", "C1 - EVSE not ready (charging interrupted)"},
        {"C2", "C2 - Charging"},
        {"D1", "D1 - EVSE not ready (ventilation required)"},
        {"D2", "D2 - Charging (ventilation required)"},
        {"E", "E - Error: Grid not available (short on CP)"},
        {"F", "F - Error: EVSE not available"},
    };
};

#endif // INTEGRATIONPLUGINALFEN_H
