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

#include "integrationpluginfoxesswbv3.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include "platform/platformzeroconfcontroller.h"
#include "network/zeroconf/zeroconfservicebrowser.h"

IntegrationPluginFoxEss::IntegrationPluginFoxEss() { }

void IntegrationPluginFoxEss::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcFoxEss()) << "FoxESS - Discovery started";
    if (info->thingClassId() == foxEssThingClassId) {
        if (!hardwareManager()->zeroConfController()->available())
        {
            qCWarning(dcFoxEss()) << "Error discovering FoxESS wallbox. Available:"  << hardwareManager()->zeroConfController()->available();
            info->finish(Thing::ThingErrorHardwareNotAvailable, "Thing discovery not possible");
            return;
        }
        m_serviceBrowser = hardwareManager()->zeroConfController()->createServiceBrowser();
        foreach (const ZeroConfServiceEntry &service, m_serviceBrowser->serviceEntries()) {
            // qCDebug(dcFoxEss()) << "mDNS service entry:" << service;
            if (service.hostName().contains("EVC-"))
            {
                ThingDescriptor descriptor(foxEssThingClassId, "FoxESS Wallbox", service.hostAddress().toString());
                ParamList params;
                // TODO: Get Mac Address
                params << Param(foxEssThingIpAddressParamTypeId, service.hostAddress().toString());
                params << Param(foxEssThingMdnsNameParamTypeId, service.name());
                params << Param(foxEssThingPortParamTypeId, service.port());
                params << Param(foxEssThingModbusIdParamTypeId, 1);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }
        }
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginFoxEss::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxEss()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxEssThingClassId) {
    }
}

void IntegrationPluginFoxEss::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcFoxEss()) << "Setup TCP connection.";
    Q_UNUSED(info)
}

void IntegrationPluginFoxEss::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcFoxEss()) << "Post setup thing..";

    if (!m_pluginTimer) {
        qCDebug(dcFoxEss()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcFoxEss()) << "Updating FoxESS EVC..";
            foreach (FoxESSModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginFoxEss::executeAction(ThingActionInfo *info)
{
    Q_UNUSED(info)
}

void IntegrationPluginFoxEss::thingRemoved(Thing *thing)
{
    qCDebug(dcFoxEss()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        FoxESSModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
