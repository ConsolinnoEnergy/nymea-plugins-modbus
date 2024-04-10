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

#include "integrationpluginsolaxevc.h"
#include "plugininfo.h"
#include "discoverytcp.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginSolaxEvc::IntegrationPluginSolaxEvc()
{

}

void IntegrationPluginSolaxEvc::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxEvcThingClassId)
    {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxEvc()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        SolaxEvcTCPDiscovery *discovery = new SolaxEvcTCPDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &SolaxEvcTCPDiscovery::discoveryFinished, info, [=](){
            foreach (const SolaxEvcTCPDiscovery::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxEvcThingClassId, "Solax Wallbox " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxEvc()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxEvcThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolaxEvc()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxEvcThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxEvcThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxEvcThingPortParamTypeId, result.port);
                params << Param(solaxEvcThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();
    }
}

void IntegrationPluginSolaxEvc::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolaxEvc()) << "Setup" << thing << thing->params();
}

void IntegrationPluginSolaxEvc::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcSolaxEvc()) << "Post setup thing..";

    if (!m_pluginTimer)
    {
        qCDebug(dcSolaxEvc()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcSolaxEvc()) << "Updating Solax EVC..";
            foreach (SolaxEvcModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginSolaxEvc::executeAction(ThingActionInfo *info)
{
    Q_UNUSED(info)
}

void IntegrationPluginSolaxEvc::thingRemoved(Thing *thing)
{
    qCDebug(dcSolaxEvc()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing))
    {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing))
    {
        SolaxEvcModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer)
    {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
