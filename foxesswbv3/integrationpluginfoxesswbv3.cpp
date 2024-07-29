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
#include "discoverytcp.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginFoxEss::IntegrationPluginFoxEss() { }

void IntegrationPluginFoxEss::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == foxEssThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcFoxEss()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature,
                         QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the
        // discovery info is done
        FoxEssTCPDiscovery *discovery =
                new FoxEssTCPDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &FoxEssTCPDiscovery::discoveryFinished, info, [=]() {
            foreach (const FoxEssTCPDiscovery::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(foxEssThingClassId, "FoxESS wallbox 3rd Gen",
                                           result.networkDeviceInfo.address().toString());
                qCInfo(dcFoxEss())
                        << "Discovered:" << descriptor.title() << descriptor.description();

                // TODO: CHECK IF RECONFIGURE WORKING
                // Check if this device has already been configured. If yes, take it's ThingId. This
                // does two things:
                // - During normal configure, the discovery won't display devices that have a
                // ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId
                // that already exists. For reconfigure to work, we need to set an already existing
                // ThingId.
                Things existingThings =
                        myThings()
                                .filterByThingClassId(foxEssThingClassId)
                                .filterByParam(foxEssThingModbusIdParamTypeId,
                                               result.networkDeviceInfo.macAddress());
                if (!existingThings.isEmpty())
                    descriptor.setThingId(existingThings.first()->id());

                ParamList params;
                params << Param(foxEssThingIpAddressParamTypeId,
                                result.networkDeviceInfo.address().toString());
                params << Param(foxEssThingMacAddressParamTypeId,
                                result.networkDeviceInfo.macAddress());
                params << Param(foxEssThingPortParamTypeId, result.port);
                params << Param(foxEssThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();
    }
}

void IntegrationPluginFoxEss::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxEss()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxEssThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcFoxEss()) << "Already have a FoxESS connection for this thing. Cleaning up "
                                     "old connection and initializing new one...";
            FoxESSModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcFoxEss()) << "Setting up a new device: " << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

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

        // If the ip address was not found in the cache, wait for the for the network discovery
        // cache to be updated
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
    }
}

void IntegrationPluginFoxEss::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcFoxEss()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    uint port = thing->paramValue(foxEssThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(foxEssThingModbusIdParamTypeId).toUInt();
    FoxESSModbusTcpConnection *connection = new FoxESSModbusTcpConnection(
            monitor->networkDeviceInfo().address(), port, modbusId, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcFoxEss()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcFoxEss()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(foxEssConnectedStateTypeId).toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::reachableChanged, thing,
            [this, connection, thing](bool reachable) {
                qCDebug(dcFoxEss()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(foxEssConnectedStateTypeId, false);
                    thing->setStateValue(foxEssCurrentPowerStateTypeId, 0);
                }
            });
}

void IntegrationPluginFoxEss::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcFoxEss()) << "Post setup thing..";

    if (!m_pluginTimer) {
        qCDebug(dcFoxEss()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
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
