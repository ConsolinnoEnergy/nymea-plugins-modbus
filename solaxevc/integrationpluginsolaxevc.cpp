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
#include <QEventLoop>

IntegrationPluginSolaxEvc::IntegrationPluginSolaxEvc() { }

void IntegrationPluginSolaxEvc::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxEvcThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxEvc()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature,
                         QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the
        // discovery info is done
        SolaxEvcTCPDiscovery *discovery =
                new SolaxEvcTCPDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &SolaxEvcTCPDiscovery::discoveryFinished, info, [=]() {
            foreach (const SolaxEvcTCPDiscovery::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxEvcThingClassId, "Solax X3 EVCharger",
                                           result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxEvc())
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
                                .filterByThingClassId(solaxEvcThingClassId)
                                .filterByParam(solaxEvcThingModbusIdParamTypeId,
                                               result.networkDeviceInfo.macAddress());
                if (!existingThings.isEmpty())
                    descriptor.setThingId(existingThings.first()->id());

                ParamList params;
                params << Param(solaxEvcThingIpAddressParamTypeId,
                                result.networkDeviceInfo.address().toString());
                params << Param(solaxEvcThingMacAddressParamTypeId,
                                result.networkDeviceInfo.macAddress());
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

    if (thing->thingClassId() == solaxEvcThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSolaxEvc()) << "Already have a Solax connection for this thing. Cleaning up "
                                     "old connection and initializing new one...";
            SolaxEvcModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolaxEvc()) << "Setting up a new device: " << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress =
                MacAddress(thing->paramValue(solaxEvcThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolaxEvc())
                    << "Failed to set up Solax wallbox because the MAC address is not valid:"
                    << thing->paramValue(solaxEvcThingMacAddressParamTypeId).toString()
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
        qCDebug(dcSolaxEvc()) << "Monitor reachable" << monitor->reachable()
                              << thing->paramValue(solaxEvcThingMacAddressParamTypeId).toString();
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

void IntegrationPluginSolaxEvc::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcSolaxEvc()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    uint port = thing->paramValue(solaxEvcThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(solaxEvcThingModbusIdParamTypeId).toUInt();
    SolaxEvcModbusTcpConnection *connection = new SolaxEvcModbusTcpConnection(
            monitor->networkDeviceInfo().address(), port, modbusId, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcSolaxEvc()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcSolaxEvc()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(solaxEvcConnectedStateTypeId).toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &SolaxEvcModbusTcpConnection::reachableChanged, thing,
            [this, connection, thing](bool reachable) {
                qCDebug(dcSolaxEvc()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(solaxEvcConnectedStateTypeId, false);
                    thing->setStateValue(solaxEvcCurrentPowerStateTypeId, 0);
                }
            });

    connect(connection, &SolaxEvcModbusTcpConnection::typePowerChanged, thing,
            [this, connection, thing](quint16 type) {
                qCDebug(dcSolaxEvc()) << "Received info about EV type.";
                if (type == 1) {
                    thing->setStateMaxValue("maxChargingCurrent", 16);
                } else {
                    thing->setStateMaxValue("maxChargingCurrent", 32);
                }
            });

    connect(connection, &SolaxEvcModbusTcpConnection::initializationFinished, thing,
            [=](bool success) {
                thing->setStateValue(solaxEvcConnectedStateTypeId, success);

                if (success) {
                    qCDebug(dcSolaxEvc()) << "Solax wallbox initialized.";
                    thing->setStateValue(solaxEvcFirmwareVersionStateTypeId,
                                         connection->firmwareVersion());
                    thing->setStateValue(solaxEvcSerialNumberStateTypeId,
                                         connection->serialNumber());
                    m_lastState = 255;
                } else {
                    qCDebug(dcSolaxEvc()) << "Solax wallbox initialization failed.";
                    // Try to reconnect to device
                    connection->reconnectDevice();
                }
            });

    // connect current charging power
    connect(connection, &SolaxEvcModbusTcpConnection::totalPowerChanged, thing,
            [thing](quint16 totalPower) {
                qCDebug(dcSolaxEvc()) << "Total charging power changed" << totalPower << "W";
                // EVC show 2-3W even when not charging, this fixes the displayed value
                if (totalPower <= 100)
                    totalPower = 0;

                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Charging") {
                    if (totalPower != 0) {
                        thing->setStateValue(solaxEvcCurrentPowerStateTypeId, totalPower);
                    }
                } else {
                    thing->setStateValue(solaxEvcCurrentPowerStateTypeId, 0);
                }
            });

    // connect energy consumption of current session
    connect(connection, &SolaxEvcModbusTcpConnection::sessionEnergyChanged, thing,
            [thing](double sessionEnergy) {
                qCDebug(dcSolaxEvc()) << "Session energy changed" << sessionEnergy << "kWh";
                thing->setStateValue(solaxEvcSessionEnergyStateTypeId, sessionEnergy);
            });

    // connect total energy consumption
    connect(connection, &SolaxEvcModbusTcpConnection::totalEnergyChanged, thing, [thing](double totalEnergy) {
                qCDebug(dcSolaxEvc()) << "Total energy changed" << totalEnergy << "kWh";
                double oldEnergy = thing->stateValue(solaxEvcTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = totalEnergy - oldEnergy;
                
                if (oldEnergy == 0 || (diffEnergy >= 0 && diffEnergy <= 100)) {
                    thing->setStateValue(solaxEvcTotalEnergyConsumedStateTypeId, totalEnergy);
                } else {
                    qCWarning(dcSolaxEvc()) << "Old total Energy: " << oldEnergy;
                    qCWarning(dcSolaxEvc()) << "New total Energy: " << totalEnergy;
                    qCWarning(dcSolaxEvc()) << "Difference to previous energy is to high; or new "
                                               "energy is 0, when is previously was not.";
                }
            });

    // connect time of current charging session
    connect(connection, &SolaxEvcModbusTcpConnection::chargingTimeChanged, thing,
            [thing](quint32 time) {
                qCDebug(dcSolaxEvc()) << "Charging Time changed" << time << "s";
                quint32 oldTime = thing->stateValue(solaxEvcChargingStateTypeId).toUInt();
                if ((time - oldTime) > 5000) {
                    qCWarning(dcSolaxEvc()) << "Old time" << oldTime;
                    qCWarning(dcSolaxEvc()) << "New time" << time;
                    qCWarning(dcSolaxEvc()) << "Difference in charging time is to high.";
                } else {
                    thing->setStateValue(solaxEvcChargingTimeStateTypeId, time);
                }
            });

    // connect max charging current
    connect(connection, &SolaxEvcModbusTcpConnection::MaxCurrentChanged, thing,
            [thing](float maxCurrent) {
                qCDebug(dcSolaxEvc()) << "Max current changed" << maxCurrent << "A";
                // Every once in a while, nonsense values are received, only change when value
                // between 6 and 16 received
                if (maxCurrent >= 6 && maxCurrent <= 32)
                    thing->setStateValue(solaxEvcMaxChargingCurrentStateTypeId, maxCurrent);
            });

    connect(connection, &SolaxEvcModbusTcpConnection::deviceModeChanged, thing,
            [this, thing, connection](quint16 mode) {
                qCDebug(dcSolaxEvc()) << "Device mode changed" << mode;
                // TODO: Test if this causes issues
                // make app follow wallbox
                if (thing->stateValue(solaxEvcPowerStateTypeId).toBool() == true) {
                    if (mode == 1) {
                        qCDebug(dcSolaxEvc()) << "Toggle charging true.";
                        toggleCharging(connection, true);
                        thing->setStateValue(solaxEvcPowerStateTypeId, true);
                    } else if (mode == 0) {
                        qCDebug(dcSolaxEvc())
                                << "Charging allowed, but car says it is not charging.";
                        // toggleCharging(connection, false);
                        // thing->setStateValue(solaxEvcPowerStateTypeId, false);
                    }
                } else {
                    qCDebug(dcSolaxEvc()) << "EV Plugged In; Charging disabled! Make sure the "
                                             "wallbox does not charge.";
                    toggleCharging(connection, false);
                    thing->setStateValue(solaxEvcPowerStateTypeId, false);
                }
            });

    connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseAChanged, thing,
            [thing](double currentPhase) {
                qCDebug(dcSolaxEvc()) << "Current PhaseA" << currentPhase << "A";
                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Charging") {
                    if (currentPhase != 0) {
                        thing->setStateValue(solaxEvcCurrentPhaseAStateTypeId, currentPhase);
                    }
                } else {
                    thing->setStateValue(solaxEvcCurrentPhaseAStateTypeId, 0);
                }
            });
    connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseBChanged, thing,
            [thing](double currentPhase) {
                qCDebug(dcSolaxEvc()) << "Current PhaseB" << currentPhase << "A";
                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Charging") {
                    if (currentPhase != 0) {
                        thing->setStateValue(solaxEvcCurrentPhaseBStateTypeId, currentPhase);
                    }
                } else {
                    thing->setStateValue(solaxEvcCurrentPhaseBStateTypeId, 0);
                }
            });
    connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseCChanged, thing,
            [thing](double currentPhase) {
                qCDebug(dcSolaxEvc()) << "Current PhaseC" << currentPhase << "A";
                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Charging") {
                    if (currentPhase != 0) {
                        thing->setStateValue(solaxEvcCurrentPhaseCStateTypeId, currentPhase);
                    }
                } else {
                    thing->setStateValue(solaxEvcCurrentPhaseCStateTypeId, 0);
                }
            });

    // connect current state of evcharger
    // connect(connection, &SolaxEvcModbusTcpConnection::stateChanged, thing, [thing](quint16 state) {
    //     qCDebug(dcSolaxEvc()) << "State changed" << state;
    //     // thing->setStateValue(solaxEvcStateStateTypeId, state);

    //     switch (state) {
    //     case SolaxEvcModbusTcpConnection::StateAvailable:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Available");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StatePreparing:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Preparing");
    //         thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateCharging:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Charging");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, true);
    //         thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateFinishing:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Plugged In");
    //         thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateFaulted:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Faulted");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         // thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateUnavailable:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Unavailable");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateReserved:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Reserved");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateSuspendedEV:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEV");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateSuspendedEVSE:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEVSE");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateUpdate:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Update");
    //         thing->setStateValue(solaxEvcChargingStateTypeId, false);
    //         break;
    //     case SolaxEvcModbusTcpConnection::StateCardActivation:
    //         thing->setStateValue(solaxEvcStateStateTypeId, "Card Activation");
    //         break;
    //     default:
    //         qCWarning(dcSolaxEvc()) << "State changed to unknown value";
    //         // thing->setStateValue(solaxEvcStateStateTypeId, "Unknown");
    //         break;
    //     }
    // });

    // connect fault code
    connect(connection, &SolaxEvcModbusTcpConnection::faultCodeChanged, thing,
            [thing](quint32 code) {
                qCDebug(dcSolaxEvc()) << "Fault code changed" << code;
                QMap<int, QString> faultCodeMap = {
                    { 0, "PowerSelect_Fault (0)" },   { 1, "Emergency Stop (1)" },
                    { 2, "Overvoltage L1 (2)" },      { 3, "Undervoltage L1 (3)" },
                    { 4, "Overvoltage L2 (4)" },      { 5, "Undervoltage L2 (5)" },
                    { 6, "Overvoltage L3 (6)" },      { 7, "Undervoltage L2 (7)" },
                    { 8, "Electronic Lock (8)" },     { 9, "Over Load (9)" },
                    { 10, "Over Current (10)" },      { 11, "Over Temperature (11)" },
                    { 12, "PE Ground (12)" },         { 13, "PE Leak Current (13)" },
                    { 14, "Over Leak Current (14)" }, { 15, "Meter Communication (15)" },
                    { 16, "485 Communication (16)" }, { 17, "CP Voltage (17)" }
                };
                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Faulted") {
                    thing->setStateValue(solaxEvcFaultCodeStateTypeId, faultCodeMap[code]);
                } else {
                    thing->setStateValue(solaxEvcFaultCodeStateTypeId, "No Error");
                }
            });

    connect(connection, &SolaxEvcModbusTcpConnection::updateFinished, thing,
            [this, thing, connection]() {
                qCDebug(dcSolaxEvc()) << "Updated finished.";

                quint16 state = connection->state();
                if (m_lastState == 255 || m_lastState == state) {
                    switch (state) {
                    case SolaxEvcModbusTcpConnection::StateAvailable:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Available");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StatePreparing:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Preparing");
                        thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateCharging:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Charging");
                        thing->setStateValue(solaxEvcChargingStateTypeId, true);
                        thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                        break;
                    case SolaxEvcModbusTcpConnection::StateFinishing:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Plugged In");
                        thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateFaulted:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Faulted");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        // thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateUnavailable:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Unavailable");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateReserved:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Reserved");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateSuspendedEV:
                        thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEV");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateSuspendedEVSE:
                        thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEVSE");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateUpdate:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Update");
                        thing->setStateValue(solaxEvcChargingStateTypeId, false);
                        break;
                    case SolaxEvcModbusTcpConnection::StateCardActivation:
                        thing->setStateValue(solaxEvcStateStateTypeId, "Card Activation");
                        break;
                    default:
                        qCWarning(dcSolaxEvc()) << "State changed to unknown value";
                        // thing->setStateValue(solaxEvcStateStateTypeId, "Unknown");
                        break;
                    }
                }
                m_lastState = state;

                double currentPhaseA =
                        thing->stateValue(solaxEvcCurrentPhaseAStateTypeId).toDouble();
                double currentPhaseB =
                        thing->stateValue(solaxEvcCurrentPhaseBStateTypeId).toDouble();
                double currentPhaseC =
                        thing->stateValue(solaxEvcCurrentPhaseCStateTypeId).toDouble();
                int phaseCount = 0;
                if (currentPhaseA > 0)
                    phaseCount++;

                if (currentPhaseB > 0)
                    phaseCount++;

                if (currentPhaseC > 0)
                    phaseCount++;

                thing->setStateValue(solaxEvcPhaseCountStateTypeId, qMax(phaseCount, 1));

                if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Charging") {
                    if (thing->stateValue(solaxEvcPowerStateTypeId).toBool() == false) {
                        qCDebug(dcSolaxEvc())
                                << "Car charging, but disabled. Toggle charging false.";
                        toggleCharging(connection, false);
                        thing->setStateValue(solaxEvcPowerStateTypeId, false);
                    }
                } else {
                    qCDebug(dcSolaxEvc()) << "EV Plugged In; Charging enabled! Make sure the "
                                             "wallbox is charging.";
                    if ((thing->stateValue(solaxEvcPowerStateTypeId).toBool() == true)
                        && (thing->stateValue(solaxEvcPluggedInStateTypeId).toBool() == true)) {
                        qCDebug(dcSolaxEvc()) << "Toggle charging true.";
                        toggleCharging(connection, true);
                        thing->setStateValue(solaxEvcPowerStateTypeId, true);
                    }
                }
            });

    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);

    return;
}

void IntegrationPluginSolaxEvc::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcSolaxEvc()) << "Post setup thing..";

    if (!m_pluginTimer) {
        qCDebug(dcSolaxEvc()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
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
    // TODO: needs to be properly tested, when modbus communication is working properly
    Thing *thing = info->thing();
    if (thing->thingClassId() == solaxEvcThingClassId) {
        SolaxEvcModbusTcpConnection *connection = m_tcpConnections.value(thing);

        // set the maximum charging current
        if (info->action().actionTypeId() == solaxEvcMaxChargingCurrentActionTypeId) {
            // set maximal charging current
            quint32 maxCurrent =
                    info->action()
                            .paramValue(
                                    solaxEvcMaxChargingCurrentActionMaxChargingCurrentParamTypeId)
                            .toUInt();
            QModbusReply *reply = connection->setMaxCurrent(maxCurrent);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, thing, [info, thing, reply, maxCurrent]() {
                if (reply->error() == QModbusDevice::NoError) {
                    thing->setStateValue(solaxEvcMaxChargingCurrentStateTypeId, maxCurrent);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcSolaxEvc())
                            << "Error setting maximal charging current:" << reply->error()
                            << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

        // toggle charging enabled
        if (info->action().actionTypeId() == solaxEvcPowerActionTypeId) {
            // start / stop the charging session
            bool power = info->action().paramValue(solaxEvcPowerActionPowerParamTypeId).toBool();
            QModbusReply *reply = connection->setControlCommand(
                    power ? SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStartCharging
                          : SolaxEvcModbusTcpConnection::ControlCommand::
                                    ControlCommandStopCharging);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, thing, [info, thing, reply, power]() {
                if (reply->error() == QModbusDevice::NoError) {
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcSolaxEvc()) << "Error setting charging state: " << reply->error()
                                            << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
                thing->setStateValue(solaxEvcPowerStateTypeId, power);
            });
        }
    }
}

void IntegrationPluginSolaxEvc::toggleCharging(SolaxEvcModbusTcpConnection *connection, bool power)
{
    QModbusReply *reply = connection->setControlCommand(
            power ? SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStartCharging
                  : SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStopCharging);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, reply, connection, power]() {
        if (reply->error() == QModbusDevice::NoError) {
        } else {
            qCWarning(dcSolaxEvc())
                    << "Error setting charging state: " << reply->error() << reply->errorString();
            toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginSolaxEvc::thingRemoved(Thing *thing)
{
    qCDebug(dcSolaxEvc()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        SolaxEvcModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
