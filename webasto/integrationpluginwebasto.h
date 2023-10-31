/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#ifndef INTEGRATIONPLUGINWEBASTO_H
#define INTEGRATIONPLUGINWEBASTO_H

#include <plugintimer.h>
#include <integrations/integrationplugin.h>
#include <network/networkdevicemonitor.h>

#include "webasto.h"
#include "webastonextmodbustcpconnection.h"
#include "evc04modbustcpconnection.h"

#include <QUuid>
#include <QObject>
#include <QHostAddress>

class IntegrationPluginWebasto : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginwebasto.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginWebasto();
    void init() override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;
    void executeAction(ThingActionInfo *info) override;

private:
    PluginTimer *m_pluginTimer = nullptr;

    QHash<QUuid, ThingActionInfo *> m_asyncActions;

    QHash<Thing *, Webasto *> m_webastoLiveConnections;
    QHash<Thing *, WebastoNextModbusTcpConnection *> m_webastoNextConnections;
    QHash<Thing *, EVC04ModbusTcpConnection *> m_evc04Connections;
    QHash<Thing *, NetworkDeviceMonitor *> m_monitors;

    void setupWebastoNextConnection(ThingSetupInfo *info);

    void update(Webasto *webasto);
    void evaluatePhaseCount(Thing *thing);

    void executeWebastoNextPowerAction(ThingActionInfo *info, bool power);

    void setupEVC04Connection(ThingSetupInfo *info);
    void updateEVC04MaxCurrent(Thing *thing, EVC04ModbusTcpConnection *connection);
    QHash<Thing *, quint32> m_lastWallboxTime;
    QHash<Thing *, quint16> m_timeoutCount;


private slots:
    void onConnectionChanged(bool connected);
    void onWriteRequestExecuted(const QUuid &requestId, bool success);
    void onWriteRequestError(const QUuid &requestId, const QString &error);

    void onReceivedRegister(Webasto::TqModbusRegister registerAddress, const QVector<quint16> &data);
};

#endif // INTEGRATIONPLUGINWEBASTO_H
