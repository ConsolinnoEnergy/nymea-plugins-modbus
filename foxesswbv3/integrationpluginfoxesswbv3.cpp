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
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcFoxEss()) << "Discovery: Network discovery finished. Found"
                              << discoveryReply->networkDeviceInfos().count() << "network devices";
            m_serviceBrowser = hardwareManager()->zeroConfController()->createServiceBrowser();
            foreach (const ZeroConfServiceEntry &service, m_serviceBrowser->serviceEntries()) {
                // qCDebug(dcFoxEss()) << "mDNS service entry:" << service;
                if (service.hostName().contains("EVC-"))
                {
                    ThingDescriptor descriptor(foxEssThingClassId, "FoxESS Wallbox", service.hostAddress().toString());
                    NetworkDeviceInfo foxESSWallbox = discoveryReply->networkDeviceInfos().get(service.hostAddress());
                    qCDebug(dcFoxEss()) << "MacAddress of WB:" << foxESSWallbox.macAddress();
                    ParamList params;
                    // TODO: Get Mac Address
                    params << Param(foxEssThingIpAddressParamTypeId, service.hostAddress().toString());
                    params << Param(foxEssThingMdnsNameParamTypeId, service.name());
                    params << Param(foxEssThingPortParamTypeId, service.port());
                    params << Param(foxEssThingMacAddressParamTypeId, foxESSWallbox.macAddress());
                    params << Param(foxEssThingModbusIdParamTypeId, 1);
                    descriptor.setParams(params);
                    info->addThingDescriptor(descriptor);
                }
            }
            discoveryReply->deleteLater();
            info->finish(Thing::ThingErrorNoError);
        });
    }
}

void IntegrationPluginFoxEss::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxEss()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxEssThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress =
                MacAddress(thing->paramValue(foxEssThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcFoxEss())
                    << "Failed to set up FoxESS wallbox because the MAC address is not valid:"
                    << thing->paramValue(foxEssThingMacAddressParamTypeId).toString()
                    << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter,
                         QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device "
                                    "to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is
        // reachable without polling on our own. In this call, nymea is checking a list for known
        // mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor =
                hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip
        // address and is set to not reachable
        m_monitors.insert(thing, monitor);
    
        qCDebug(dcFoxEss()) << "Monitor reachable" << monitor->reachable()
                              << thing->paramValue(foxEssThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(),
                    &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                        if (!m_setupTcpConnectionRunning) {
                            m_setupTcpConnectionRunning = true;
                            setupTcpConnection(info);
                        }
                    });
        }
        info->finish(Thing::ThingErrorNoError);
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
