/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2024, nymea GmbH
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

#include "integrationpluginsungrowsh.h"
#include "plugininfo.h"
#include "sungrowdiscovery.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginSungrow::IntegrationPluginSungrow()
{

}

void IntegrationPluginSungrow::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSungrow()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
    SungrowDiscovery *discovery = new SungrowDiscovery(hardwareManager()->networkDeviceDiscovery(), m_modbusTcpPort, m_modbusSlaveAddress, info);
    connect(discovery, &SungrowDiscovery::discoveryFinished, discovery, &SungrowDiscovery::deleteLater);
    connect(discovery, &SungrowDiscovery::discoveryFinished, info, [=](){
        foreach (const SungrowDiscovery::SungrowDiscoveryResult &result, discovery->discoveryResults()) {
            QString title = "Sungrow " + QString::number(result.nominalOutputPower) + "kW Inverter";

            if (!result.serialNumber.isEmpty())
                title.append(" " + result.serialNumber);

            ThingDescriptor descriptor(sungrowInverterTcpThingClassId, title, result.networkDeviceInfo.address().toString() + " " + result.networkDeviceInfo.macAddress());
            qCInfo(dcSungrow()) << "Discovered:" << descriptor.title() << descriptor.description();

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(sungrowInverterTcpThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcSungrow()) << "This Sungrow inverter already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(sungrowInverterTcpThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    });

    // Start the discovery process
    discovery->startDiscovery();
}

void IntegrationPluginSungrow::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCInfo(dcSungrow()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == sungrowInverterTcpThingClassId) {

        // Handle reconfiguration
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSungrow()) << "Reconfiguring existing thing" << thing->name();
            m_tcpConnections.take(thing)->deleteLater();
            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        MacAddress macAddress = MacAddress(thing->paramValue(sungrowInverterTcpThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcSungrow()) << "The configured MAC address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure this inverter."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Clean up in case the setup gets aborted
            if (m_monitors.contains(thing)) {
                qCDebug(dcSungrow()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();

        qCInfo(dcSungrow()) << "Setting up Sungrow on" << address.toString();
        auto sungrowConnection = new SungrowModbusTcpConnection(address, m_modbusTcpPort , m_modbusSlaveAddress, this);
        connect(info, &ThingSetupInfo::aborted, sungrowConnection, &SungrowModbusTcpConnection::deleteLater);

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSungrow()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->setupComplete())
                return;

            if (reachable && !thing->stateValue("connected").toBool()) {
                sungrowConnection->setHostAddress(monitor->networkDeviceInfo().address());
                sungrowConnection->reconnectDevice();
            } else if (!reachable) {
                // Note: Auto reconnect is disabled explicitly and
                // the device will be connected once the monitor says it is reachable again
                sungrowConnection->disconnectDevice();
            }
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::reachableChanged, thing, [this, thing, sungrowConnection](bool reachable){
            qCInfo(dcSungrow()) << "Reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                sungrowConnection->initialize();
            } else {
                thing->setStateValue("connected", false);
                thing->setStateValue(sungrowInverterTcpCurrentPowerStateTypeId, 0);

                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }

                Thing *child = getMeterThing(thing);
                if (child) {
                    child->setStateValue(sungrowMeterCurrentPowerStateTypeId, 0);
                    child->setStateValue(sungrowMeterCurrentPhaseAStateTypeId, 0);
                    child->setStateValue(sungrowMeterCurrentPhaseBStateTypeId, 0);
                    child->setStateValue(sungrowMeterCurrentPhaseCStateTypeId, 0);
                }

                child = getBatteryThing(thing);
                if (child) {
                    child->setStateValue(sungrowBatteryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue("connected", success);

            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", success);
            }

            if (!success) {
                // Try once to reconnect the device
                sungrowConnection->reconnectDevice();
            } else {
                qCInfo(dcSungrow()) << "Connection initialized successfully for" << thing;
                thing->setStateValue(sungrowInverterTcpNominalPowerStateTypeId, sungrowConnection->nominalOutputPower()*1000);

                Thing *child = getBatteryThing(thing);
                if (child) {
                    child->setStateValue(sungrowBatteryNominalPowerBatteryStateTypeId, sungrowConnection->batteryNominalPower());
                }
                sungrowConnection->update();
            }
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcSungrow()) << "Updated" << sungrowConnection;

            if (myThings().filterByParentId(thing->id()).filterByThingClassId(sungrowMeterThingClassId).isEmpty()) {
                qCDebug(dcSungrow()) << "There is no meter set up for this inverter. Creating a meter for" << thing << sungrowConnection;
                ThingClass meterThingClass = thingClass(sungrowMeterThingClassId);
                ThingDescriptor descriptor(sungrowMeterThingClassId, meterThingClass.displayName() + " " + sungrowConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Check if a battery is connected to this Sungrow inverter
            if (sungrowConnection->batteryTemperature() != 0 &&
                    myThings().filterByParentId(thing->id()).filterByThingClassId(sungrowBatteryThingClassId).isEmpty()) {
                qCDebug(dcSungrow()) << "There is a battery connected but not set up yet. Creating a battery.";
                ThingClass batteryThingClass = thingClass(sungrowBatteryThingClassId);
                ThingDescriptor descriptor(sungrowBatteryThingClassId, batteryThingClass.displayName() + " " + sungrowConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Update inverter states
            thing->setStateValue(sungrowInverterTcpCurrentPowerStateTypeId, static_cast<double>(sungrowConnection->totalPVPower()) * -1);
            thing->setStateValue(sungrowInverterTcpTemperatureStateTypeId, sungrowConnection->inverterTemperature());
            thing->setStateValue(sungrowInverterTcpFrequencyStateTypeId, sungrowConnection->gridFrequency());
            thing->setStateValue(sungrowInverterTcpTotalEnergyProducedStateTypeId, sungrowConnection->totalPVGeneration());

            // Update the meter if available
            Thing *meterThing = getMeterThing(thing);
            if (meterThing) {
                quint16 runningState = sungrowConnection->runningState();
                qCDebug(dcSungrow()) << "Power generated from PV:" << (runningState & (0x1 << 0) ? "true" : "false");
                qCDebug(dcSungrow()) << "Battery charging:" << (runningState & (0x1 << 1) ? "true" : "false");
                qCDebug(dcSungrow()) << "Battery discharging:" << (runningState & (0x1 << 2) ? "true" : "false");
                qCDebug(dcSungrow()) << "Positive load power:" << (runningState & (0x1 << 3) ? "true" : "false");
                qCDebug(dcSungrow()) << "Feed-in power:" << (runningState & (0x1 << 4) ? "true" : "false");
                qCDebug(dcSungrow()) << "Import power from grid:" << (runningState & (0x1 << 5) ? "true" : "false");
                qCDebug(dcSungrow()) << "Negative load power:" << (runningState & (0x1 << 7) ? "true" : "false");
                meterThing->setStateValue(sungrowMeterCurrentPowerStateTypeId, sungrowConnection->exportPower() * -1);
                meterThing->setStateValue(sungrowMeterTotalEnergyConsumedStateTypeId, sungrowConnection->totalImportEnergy());
                meterThing->setStateValue(sungrowMeterTotalEnergyProducedStateTypeId, sungrowConnection->totalExportEnergy());
                meterThing->setStateValue(sungrowMeterCurrentPhaseAStateTypeId, sungrowConnection->phaseACurrent());
                meterThing->setStateValue(sungrowMeterCurrentPhaseBStateTypeId, sungrowConnection->phaseBCurrent());
                meterThing->setStateValue(sungrowMeterCurrentPhaseCStateTypeId, sungrowConnection->phaseCCurrent());
                meterThing->setStateValue(sungrowMeterVoltagePhaseAStateTypeId, sungrowConnection->phaseAVoltage());
                meterThing->setStateValue(sungrowMeterVoltagePhaseBStateTypeId, sungrowConnection->phaseBVoltage());
                meterThing->setStateValue(sungrowMeterVoltagePhaseCStateTypeId, sungrowConnection->phaseCVoltage());
                meterThing->setStateValue(sungrowMeterFrequencyStateTypeId, sungrowConnection->gridFrequency());
            }

            // Update the battery if available
            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                batteryThing->setStateValue(sungrowBatteryVoltageStateTypeId, sungrowConnection->batteryVoltage());
                batteryThing->setStateValue(sungrowBatteryTemperatureStateTypeId, sungrowConnection->batteryTemperature());
                batteryThing->setStateValue(sungrowBatteryBatteryLevelStateTypeId, sungrowConnection->batteryLevel());
                batteryThing->setStateValue(sungrowBatteryBatteryCriticalStateTypeId, sungrowConnection->batteryLevel() < 5);
                batteryThing->setStateValue(sungrowBatteryCapacityStateTypeId, sungrowConnection->totalBatteryCapacity());

                // Note: since firmware 2024 this is a int16 value, and we can use the value directly without convertion
                if (sungrowConnection->batteryPower() < 0) {
                    batteryThing->setStateValue(sungrowBatteryCurrentPowerStateTypeId, -1*sungrowConnection->batteryPower());
                } else {
                    qint16 batteryPower = (sungrowConnection->runningState() & (0x1 << 1) ? sungrowConnection->batteryPower() : sungrowConnection->batteryPower() * -1);
                    batteryThing->setStateValue(sungrowBatteryCurrentPowerStateTypeId, batteryPower);
                }

                quint16 runningState = sungrowConnection->runningState();
                if (runningState & (0x1 << 1)) { //Bit 1: Battery charging bit
                    batteryThing->setStateValue(sungrowBatteryChargingStateStateTypeId, "charging");
                } else if (runningState & (0x1 << 2)) { //Bit 2: Battery discharging bit
                    batteryThing->setStateValue(sungrowBatteryChargingStateStateTypeId, "discharging");
                } else {
                    batteryThing->setStateValue(sungrowBatteryChargingStateStateTypeId, "idle");
                }
            }

            // Mode: 170 - ON | Export will be limited to chosen value
            //       85 - OFF | Export not limited, max export
            // Check if mode is 85. If yes, set to 170
            if (sungrowConnection->exportLimitMode() == InverterLimitation::UNLIMITED) {
                qCDebug(dcSungrow()) << "Export is unlimited, try to limit it";
                QModbusReply *reply = sungrowConnection->setExportLimitMode(InverterLimitation::LIMITED);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, thing, [reply]() {
                    qCDebug(dcSungrow()) << "Set export mode finished";
                    if (reply->error() != QModbusDevice::NoError) {
                        qCDebug(dcSungrow()) << "Error setting export limit mode";
                    } else {
                        qCWarning(dcSungrow()) << "Successfully set export limit mode";
                    }
                });
            }

            // quint16 batteryCommand = sungrowConnection->chargeCommand();
            // if (batteryThing) {
            //     if (batteryCommand == BatteryControl::FORCE_STOP) {
            //         batteryThing->setStateValue(sungrowBatteryEnableForcePowerStateStateTypeId, false);
            //     } else {
            //         batteryThing->setStateValue(sungrowBatteryEnableForcePowerStateStateTypeId, true);
            //     }
            // }
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::exportLimitChanged, thing, [thing](quint16 exportLimit) {
            qCDebug(dcSungrow()) << "Export limit changed to" << exportLimit << "W";
            thing->setStateValue(sungrowInverterTcpExportLimitStateTypeId, exportLimit);
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::chargeCommandChanged, thing, [this, thing](quint16 command) {
            qCDebug(dcSungrow()) << "Charge command changed to" << command;

            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                if (command == BatteryControl::FORCE_STOP) {
                    batteryThing->setStateValue(sungrowBatteryEnableForcePowerStateStateTypeId, false);
                } else {
                    batteryThing->setStateValue(sungrowBatteryEnableForcePowerStateStateTypeId, true);
                }
            }
        });

        connect(sungrowConnection, &SungrowModbusTcpConnection::batteryMinLevelChanged, thing, [this, thing] (float level) {
            qCDebug(dcSungrow()) << "Min battery level changed to" << level;

            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                batteryThing->setStateValue(sungrowBatteryMinBatteryLevelStateTypeId, (uint) level);
            }
        });

        m_tcpConnections.insert(thing, sungrowConnection);

        if (monitor->reachable())
            sungrowConnection->connectDevice();

        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == sungrowMeterThingClassId) {

        // Get the parent thing and the associated connection
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcSungrow()) << "Failed to set up Sungrow energy meter because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        auto sungrowConnection = m_tcpConnections.value(connectionThing);
        if (!sungrowConnection) {
            qCWarning(dcSungrow()) << "Failed to set up Sungrow energy meter because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated
        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == sungrowBatteryThingClassId) {
        // Get the parent thing and the associated connection
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcSungrow()) << "Failed to set up Sungrow battery because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        auto sungrowConnection = m_tcpConnections.value(connectionThing);
        if (!sungrowConnection) {
            qCWarning(dcSungrow()) << "Failed to set up Sungrow battery because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated
        info->finish(Thing::ThingErrorNoError);
        return;
    }
}

void IntegrationPluginSungrow::postSetupThing(Thing *thing)
{

    if (thing->thingClassId() == sungrowInverterTcpThingClassId) {

        // Create the update timer if not already set up
        if (!m_refreshTimer) {
            qCDebug(dcSungrow()) << "Starting plugin timer...";
            m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
            connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
                foreach(auto thing, myThings().filterByThingClassId(sungrowInverterTcpThingClassId)) {
                    auto monitor = m_monitors.value(thing);
                    if (!monitor->reachable()) {
                        continue;
                    }

                    auto connection = m_tcpConnections.value(thing);

                    if (connection->reachable()) {
                        qCDebug(dcSungrow()) << "Updating connection" << connection->hostAddress().toString();
                        connection->update();
                    } else {
                        qCDebug(dcSungrow()) << "Device not reachable. Probably a TCP connection error. Reconnecting TCP socket";
                        connection->reconnectDevice();
                    }
                }
            });
            m_refreshTimer->start();
        }
        return;
    }

    if (thing->thingClassId() == sungrowMeterThingClassId || thing->thingClassId() == sungrowBatteryThingClassId) {
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (connectionThing) {
            thing->setStateValue("connected", connectionThing->stateValue("connected"));
        }
        return;
    }
}

void IntegrationPluginSungrow::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == sungrowInverterTcpThingClassId && m_tcpConnections.contains(thing)) {
        auto connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    // Unregister related hardware resources
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcSungrow()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

Thing *IntegrationPluginSungrow::getMeterThing(Thing *parentThing)
{
    Things meterThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(sungrowMeterThingClassId);
    if (meterThings.isEmpty())
        return nullptr;

    return meterThings.first();
}

Thing *IntegrationPluginSungrow::getBatteryThing(Thing *parentThing)
{
    Things batteryThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(sungrowBatteryThingClassId);
    if (batteryThings.isEmpty())
        return nullptr;

    return batteryThings.first();
}

// TODO: Define min and max SOC
void IntegrationPluginSungrow::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == sungrowInverterTcpThingClassId) {
        SungrowModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSungrow()) << "executeAction - Modbus connection not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSungrow()) << "Could not execute action. Modbus connection is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == sungrowInverterTcpSetExportLimitActionTypeId) {
            double ratedPower = thing->stateValue(sungrowInverterTcpNominalPowerStateTypeId).toDouble();
            quint16 powerLimit = action.paramValue(sungrowInverterTcpSetExportLimitActionExportLimitParamTypeId).toUInt();
            quint16 targetPowerLimit = powerLimit * (ratedPower/100);

            QModbusReply *reply = connection->setExportLimit(targetPowerLimit);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, reply, [info, reply, targetPowerLimit] (){
                qCWarning(dcSungrow()) << "Set export limit - Received reply";
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSungrow()) << "Error settting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCWarning(dcSungrow()) << "Successfully set export limit to " << targetPowerLimit;
                    info->finish(Thing::ThingErrorNoError);
                }
            });

        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }

    } else if (thing->thingClassId() == sungrowBatteryThingClassId) {
        Thing *inverterThing = myThings().findById(thing->parentId());
        SungrowModbusTcpConnection *connection = m_tcpConnections.value(inverterThing);
        if (!connection) {
            qCWarning(dcSungrow()) << "executeAction - Modbus connection not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSungrow()) << "Could not execute action. Modbus connection is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == sungrowBatteryEnableForcePowerActionTypeId) {
            // TODO: If battery should be force
            // Check if EMS Mode is 2 or 3
            //       else set to 2 or 3 (?)
            // Get current configured power from state
            // Depending on sign, set the command

            bool power = action.paramValue(sungrowBatteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            double targetPower = thing->stateValue(sungrowBatteryForcePowerStateTypeId).toDouble();
            quint16 mode = power ? 2 : 0;

            // Set EMS Mode
            qCDebug(dcSungrow()) << "Trying to set EMS mode to" << mode << ". 0: Internal control, 2: External control.";
            QModbusReply *reply = connection->setEmsModeSelection(mode);
            if (power != thing->stateValue(sungrowBatteryEnableForcePowerStateTypeId).toBool()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, reply, [info, thing, power, connection, reply, mode, targetPower] (){
                    qCWarning(dcSungrow()) << "Set ems mode - Received reply";
                    if (reply->error() != QModbusDevice::NoError) {
                        qCWarning(dcSungrow()) << "Set ems mode - Error setting ems mode" << reply->error() << reply->errorString();
                        thing->setStateValue(sungrowBatteryEnableForcePowerStateTypeId, !power);
                        info->finish(Thing::ThingErrorHardwareFailure);
                        connection->reconnectDevice();
                    } else {
                        qCDebug(dcSungrow()) << "Set ems mode - Successfully set ems mode to" << mode;
                        quint16 command = 0;
                        if (power == false) {
                            command = BatteryControl::FORCE_STOP;
                        } else {
                            if (targetPower > 0) {
                                command = BatteryControl::FORCE_CHARGE;
                            } else {
                                command = BatteryControl::FORCE_DISCHARGE;
                            }
                        }
                        // Set charge command
                        QModbusReply *replyCharge = connection->setChargeCommand(command);
                        connect(replyCharge, &QModbusReply::finished, replyCharge, &QModbusReply::deleteLater);
                        connect(replyCharge, &QModbusReply::finished, replyCharge, [info, thing, replyCharge, command, power, connection] (){
                            qCWarning(dcSungrow()) << "Set battery state - Received reply";
                            if (replyCharge->error() != QModbusDevice::NoError) {
                                qCWarning(dcSungrow()) << "Set battery state - Error setting ems mode" << replyCharge->error() << replyCharge->errorString();
                                thing->setStateValue(sungrowBatteryEnableForcePowerStateTypeId, !power);
                                info->finish(Thing::ThingErrorHardwareFailure);
                                connection->reconnectDevice();
                            } else {
                                qCDebug(dcSungrow()) << "Set battery state - Successfully set battery state to" << command;
                                thing->setStateValue(sungrowBatteryEnableForcePowerStateTypeId, power);
                                info->finish(Thing::ThingErrorNoError);
                            }
                        });
                    }
                });
            }

        } else if (action.actionTypeId() == sungrowBatteryForcePowerActionTypeId) {
            // TODO: Do not set command to FORCE_STOP, only set CHARGE / DISCHARGE

            double targetPower = action.paramValue(sungrowBatteryForcePowerActionForcePowerParamTypeId).toDouble();
            bool power = thing->stateValue(sungrowBatteryEnableForcePowerStateTypeId).toBool();
            double nominalBatteryPower = thing->stateValue(sungrowBatteryNominalPowerBatteryStateTypeId).toDouble();
            double oldTargetPower = thing->stateValue(sungrowBatteryForcePowerStateTypeId).toDouble();

            if (targetPower != oldTargetPower) {
                if (qAbs(targetPower) > qAbs(nominalBatteryPower))
                    targetPower = nominalBatteryPower - 3000;

                qCDebug(dcSungrow()) << "Current set chargePower is" << connection->chargePower() << "while targetPower is" << qAbs(targetPower);
                if (connection->chargePower() != qAbs(targetPower)) {
                    qCDebug(dcSungrow()) << "Trying to set battery power";
                    QModbusReply *reply = connection->setChargePower(qAbs(targetPower));

                    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                    connect(reply, &QModbusReply::finished, reply, [info, thing, reply, targetPower, power, connection] (){
                        qCWarning(dcSungrow()) << "Set battery power - Received reply";
                        if (reply->error() != QModbusDevice::NoError) {
                            qCWarning(dcSungrow()) << "Set battery power - Error setting battery power" << reply->error() << reply->errorString();
                            info->finish(Thing::ThingErrorHardwareFailure);
                            connection->reconnectDevice();
                        } else {
                            qCDebug(dcSungrow()) << "Set battery power - Successfully set battery power to" << targetPower;
                            if (power == false) {
                                 thing->setStateValue(sungrowBatteryForcePowerStateTypeId, targetPower);
                                 info->finish(Thing::ThingErrorNoError);
                            } else {
                                quint16 command = 0;
                                if (targetPower > 0) {
                                    command = BatteryControl::FORCE_CHARGE;
                                } else {
                                    command = BatteryControl::FORCE_DISCHARGE;
                                }
                                // Set charge command
                                QModbusReply *replyCharge = connection->setChargeCommand(command);
                                connect(replyCharge, &QModbusReply::finished, replyCharge, &QModbusReply::deleteLater);
                                connect(replyCharge, &QModbusReply::finished, replyCharge, [info, thing, targetPower, replyCharge, command, connection] (){
                                    qCWarning(dcSungrow()) << "Set battery state - Received reply";
                                    if (replyCharge->error() != QModbusDevice::NoError) {
                                        qCWarning(dcSungrow()) << "Set battery state - Error setting ems mode" << replyCharge->error() << replyCharge->errorString();
                                        info->finish(Thing::ThingErrorHardwareFailure);
                                        connection->reconnectDevice();
                                    } else {
                                        qCDebug(dcSungrow()) << "Set battery state - Successfully set battery state to" << command;
                                        thing->setStateValue(sungrowBatteryForcePowerStateTypeId, targetPower);
                                        info->finish(Thing::ThingErrorNoError);
                                    }
                                });
                            }
                        }
                    });
                }
            }

        } else if (action.actionTypeId() == sungrowBatteryMinBatteryLevelActionTypeId) {
            quint16 minLevel = action.paramValue(sungrowBatteryMinBatteryLevelActionMinBatteryLevelParamTypeId).toUInt();

            QModbusReply *reply = connection->setBatteryMinLevel(minLevel);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, reply, [info, reply, minLevel] (){
                qCWarning(dcSungrow()) << "Set min battery level - Received reply";
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSungrow()) << "Error: Set min battery level" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCWarning(dcSungrow()) << "Set min battery level to" << minLevel;
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
    }
}
