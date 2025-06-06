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

#include "integrationpluginsolaxevcg2.h"
#include "plugininfo.h"
#include "discoverytcp.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <QEventLoop>

IntegrationPluginSolaxEvcG2::IntegrationPluginSolaxEvcG2() { }

void IntegrationPluginSolaxEvcG2::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() != solaxEvcG2ThingClassId) {
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class ID."));
        return;
    }
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSolaxEvcG2()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature,
                     QT_TR_NOOP("The network device discovery is not available."));
        return;
    }
    // Create a discovery with the info as parent for auto deleting the object once the
    // discovery info is done
    SolaxEvcG2TCPDiscovery *discovery =
            new SolaxEvcG2TCPDiscovery{ hardwareManager()->networkDeviceDiscovery(), info };
    connect(discovery, &SolaxEvcG2TCPDiscovery::discoveryFinished, info, [=]() {
        foreach (const SolaxEvcG2TCPDiscovery::Result &result, discovery->discoveryResults()) {
            ThingDescriptor descriptor{
                solaxEvcG2ThingClassId,
                        supportedThings().findById(solaxEvcG2ThingClassId).displayName(),
                        result.networkDeviceInfo.address().toString()
            };
            qCInfo(dcSolaxEvcG2()) << "Discovered:" << descriptor.title() << descriptor.description();

            // Check if this device has already been configured. If yes, take it's ThingId. This
            // does two things:
            // - During normal configure, the discovery won't display devices that have a
            // ThingId that already exists. So this prevents a device from beeing added twice.
            // - During reconfigure, the discovery only displays devices that have a ThingId
            // that already exists. For reconfigure to work, we need to set an already existing
            // ThingId.
            Things existingThings =
                    myThings()
                    .filterByThingClassId(solaxEvcG2ThingClassId)
                    .filterByParam(solaxEvcG2ThingMacAddressParamTypeId,
                                   result.networkDeviceInfo.macAddress());
            if (!existingThings.isEmpty()) {
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(solaxEvcG2ThingIpAddressParamTypeId,
                            result.networkDeviceInfo.address().toString());
            params << Param(solaxEvcG2ThingMacAddressParamTypeId,
                            result.networkDeviceInfo.macAddress());
            params << Param(solaxEvcG2ThingPortParamTypeId, result.port);
            params << Param(solaxEvcG2ThingModbusIdParamTypeId, result.modbusId);
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }
        info->finish(Thing::ThingErrorNoError);
    });

    // Start the discovery process
    discovery->startDiscovery();
}

void IntegrationPluginSolaxEvcG2::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() != solaxEvcG2ThingClassId) {
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class ID."));
        return;
    }

    qCDebug(dcSolaxEvcG2()) << "Setup" << thing << thing->params();

    // Handle reconfigure
    if (m_tcpConnections.contains(thing)) {
        qCDebug(dcSolaxEvcG2()) << "Already have a Solax connection for this thing. Cleaning up "
                                   "old connection and initializing new one...";
        SolaxEvcG2ModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    } else {
        qCDebug(dcSolaxEvcG2()) << "Setting up a new device: " << thing->params();
    }

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
    // possible. Testing for null is necessary, because registering a monitor with a zero mac
    // adress will cause a segfault.
    const auto macAddress =
            MacAddress{ thing->paramValue(solaxEvcG2ThingMacAddressParamTypeId).toString() };
    if (macAddress.isNull()) {
        qCWarning(dcSolaxEvcG2())
                << "Failed to set up Solax wallbox because the MAC address is not valid:"
                << thing->paramValue(solaxEvcG2ThingMacAddressParamTypeId).toString()
                << macAddress.toString();
        info->finish(Thing::ThingErrorInvalidParameter,
                     QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
        return;
    }

    // Create a monitor so we always get the correct IP in the network and see if the device is
    // reachable without polling on our own. In this call, nymea is checking a list for known
    // mac addresses and associated ip addresses
    const auto monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
    // If the mac address is not known, nymea is starting a internal network discovery.
    // 'monitor' is returned while the discovery is still running -> monitor does not include ip
    // address and is set to not reachable
    m_monitors.insert(thing, monitor);
    // If the ip address was not found in the cache, wait for the for the network discovery
    // cache to be updated
    qCDebug(dcSolaxEvcG2())
            << "Monitor reachable"
            << monitor->reachable()
            << macAddress.toString();
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

void IntegrationPluginSolaxEvcG2::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcSolaxEvcG2()) << "Post setup thing";
    if (!m_pluginTimer) {
        qCDebug(dcSolaxEvcG2()) << "Starting plugin timer";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcSolaxEvcG2()) << "Updating Solax EVC";
            foreach (SolaxEvcG2ModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginSolaxEvcG2::executeAction(ThingActionInfo *info)
{
    Q_UNUSED(info);
    // #TODO
}

void IntegrationPluginSolaxEvcG2::thingRemoved(Thing *thing)
{
    qCDebug(dcSolaxEvcG2()) << "Thing removed" << thing->name();
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }
    if (m_tcpConnections.contains(thing)) {
        SolaxEvcG2ModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }
    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSolaxEvcG2::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcSolaxEvcG2()) << "Setup TCP connection.";
    const auto thing = info->thing();
    const auto monitor = m_monitors.value(thing);
    uint port = thing->paramValue(solaxEvcG2ThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(solaxEvcG2ThingModbusIdParamTypeId).toUInt();
    const auto connection = new SolaxEvcG2ModbusTcpConnection {
            monitor->networkDeviceInfo().address(),
            port,
            modbusId,
            this };
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcSolaxEvcG2()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcSolaxEvcG2()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(solaxEvcG2ConnectedStateTypeId).toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::reachableChanged, thing,
            [connection, thing](bool reachable) {
                qCDebug(dcSolaxEvcG2()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(solaxEvcG2ConnectedStateTypeId, false);
                    thing->setStateValue(solaxEvcG2CurrentPowerStateTypeId, 0);
                }
            });

    // #TODO  Handle typePowerChanged (set maxChargingCurrent max value)
//    connect(connection, &SolaxEvcModbusTcpConnection::typePowerChanged, thing,
//            [this, connection, thing](quint16 type) {
//                qCDebug(dcSolaxEvc()) << "Received info about EV type.";
//                if (type == 1) {
//                    thing->setStateMaxValue("maxChargingCurrent", 16);
//                } else {
//                    thing->setStateMaxValue("maxChargingCurrent", 32);
//                }
//            });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::initializationFinished, thing,
            [=](bool success) {
                thing->setStateValue(solaxEvcG2ConnectedStateTypeId, success);

                if (success) {
                    qCDebug(dcSolaxEvcG2()) << "Solax wallbox initialized.";
                    thing->setStateValue(solaxEvcG2FirmwareVersionStateTypeId,
                                         connection->firmwareVersion());
                    // #TODO set other readSchedule=init values to states
                    // #TODO needed? m_lastState = 255;
                } else {
                    qCDebug(dcSolaxEvcG2()) << "Solax wallbox initialization failed.";
                    // Try to reconnect to device
                    connection->reconnectDevice();
                }
            });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::updateFinished,
            thing, [this, connection, thing]() { updateStateValues(connection, thing); });

    if (monitor->reachable()) {
        connection->connectDevice();
    }
    info->finish(Thing::ThingErrorNoError);
    return;
}

void IntegrationPluginSolaxEvcG2::updateStateValues(SolaxEvcG2ModbusTcpConnection *connection,
                                                    Thing *thing)
{
    Q_UNUSED(connection);
    Q_UNUSED(thing);
    // #TODO
}
