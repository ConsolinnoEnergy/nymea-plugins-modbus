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

#include "integrationpluginvictron.h"
#include "plugininfo.h"
#include "victrondiscovery.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginVictron::IntegrationPluginVictron()
{

}

void IntegrationPluginVictron::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcVictron()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
    VictronDiscovery *discovery = new VictronDiscovery(hardwareManager()->networkDeviceDiscovery(), 1502, 71, info);
    connect(discovery, &VictronDiscovery::discoveryFinished, info, [=](){
        foreach (const VictronDiscovery::VictronDiscoveryResult &result, discovery->discoveryResults()) {

            ThingDescriptor descriptor(victronInverterThingClassId, result.manufacturerName + " " + result.productName, "Serial: " + result.serialNumber + " - " + result.networkDeviceInfo.address().toString());
            qCDebug(dcVictron()) << "Discovered:" << descriptor.title() << descriptor.description();

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(victronInverterThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcVictron()) << "This Victron inverter already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(victronInverterThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            // Note: if we discover also the port and modbusaddress, we must fill them in from the discovery here, for now everywhere the defaults...
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    });

    // Start the discovery process
    discovery->startDiscovery();
}

void IntegrationPluginVictron::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcVictron()) << "Setup" << thing << thing->params();

    // Inverter (connection)
    if (thing->thingClassId() == victronInverterThingClassId) {

        // Handle reconfigure
        if (m_victronConnections.contains(thing)) {
            qCDebug(dcVictron()) << "Reconfiguring existing thing" << thing->name();
            m_victronConnections.take(thing)->deleteLater();

            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        MacAddress macAddress = MacAddress(thing->paramValue(victronInverterThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcVictron()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the thing."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);

        QHostAddress address = monitor->networkDeviceInfo().address();
        if (address.isNull()) {
            qCWarning(dcVictron()) << "Cannot set up thing. The host address is not known yet. Maybe it will be available in the next run...";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The host address is not known yet. Trying later again."));
            return;
        }

        // Clean up in case the setup gets aborted
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcVictron()) << "Unregister monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // Wait for the monitor to be ready
        if (monitor->reachable()) {
            // Thing already reachable...let's continue with the setup
            setupVictronConnection(info);
        } else {
            qCDebug(dcVictron()) << "Waiting for the network monitor to get reachable before continue to set up the connection" << thing->name() << address.toString() << "...";
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                if (reachable) {
                    qCDebug(dcVictron()) << "The monitor for thing setup" << thing->name() << "is now reachable. Continue setup...";
                    setupVictronConnection(info);
                }
            });
        }

        return;
    }

    // Meter
    if (thing->thingClassId() == victronMeterThingClassId) {
        // Get the parent thing and the associated connection
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcVictron()) << "Failed to set up victron energy meter because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        VictronModbusTcpConnection *victronConnection = m_victronConnections.value(connectionThing);
        if (!victronConnection) {
            qCWarning(dcVictron()) << "Failed to set up victron energy meter because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated

        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == victronBatteryThingClassId) {
        // Get the parent thing and the associated connection
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (!connectionThing) {
            qCWarning(dcVictron()) << "Failed to set up victron battery because the parent thing with ID" << thing->parentId().toString() << "could not be found.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        VictronModbusTcpConnection *victronConnection = m_victronConnections.value(connectionThing);
        if (!victronConnection) {
            qCWarning(dcVictron()) << "Failed to set up victron battery because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated

        info->finish(Thing::ThingErrorNoError);
        return;
    }
}

void IntegrationPluginVictron::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == victronInverterThingClassId) {

        // Create the update timer if not already set up
        if (!m_pluginTimer) {
            qCDebug(dcVictron()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(VictronModbusTcpConnection *connection, m_victronConnections) {
                    qCDebug(dcVictron()) << "Update connection" << connection->hostAddress().toString();
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        VictronModbusTcpConnection *victronConnection = m_victronConnections.value(thing);

        // Check if we have to create the meter for the Victron inverter
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(victronMeterThingClassId).isEmpty()) {
            qCDebug(dcVictron()) << "--> Read block \"powerMeterValues\" registers from:" << 220 << "size:" << 38;
            QModbusReply *reply = victronConnection->readBlockPowerMeterValues();
            if (!reply) {
                qCWarning(dcVictron()) << "Error occurred while reading block \"powerMeterValues\" registers";
                return;
            }

            if (reply->isFinished()) {
                reply->deleteLater(); // Broadcast reply returns immediatly
                return;
            }

            connect(reply, &QModbusReply::finished, this, [=](){
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    const QVector<quint16> blockValues = unit.values();

                    bool notZero = false;
                    for (int i = 0; i < blockValues.size(); i++) {
                        if (blockValues.at(i) != 0) {
                            notZero = true;
                            break;
                        }
                    }

                    if (notZero) {
                        qCDebug(dcVictron()) << "There is a meter connected but not set up yet. Creating a meter...";
                        // No meter thing created for this inverter, lets create one with the inverter as parent
                        ThingClass meterThingClass = thingClass(victronMeterThingClassId);
                        ThingDescriptor descriptor(victronMeterThingClassId, meterThingClass.displayName(), QString(), thing->id());
                        // No params required, all we need is the connection
                        emit autoThingsAppeared(ThingDescriptors() << descriptor);
                    } else {
                        qCDebug(dcVictron()) << "There is no meter connected to the inverter" << thing;
                    }
                }
            });

            connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
                qCWarning(dcVictron()) << "Modbus reply error occurred while updating block \"powerMeterValues\" registers" << error << reply->errorString();
            });
        }

        // Check if we have to create the battery for the Victron inverter
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(victronBatteryThingClassId).isEmpty()) {
            if (victronConnection->batteryType() == VictronModbusTcpConnection::BatteryTypeNoBattery) {
                qCDebug(dcVictron()) << "There is no battery connected to the inverter" << thing;
                return;
            }

            qCDebug(dcVictron()) << "There is a battery connected but not set up yet. Creating a battery" << victronConnection->batteryType();
            QString batteryName = victronConnection->batteryManufacturer();
            if (victronConnection->batteryModelId() != 0) {
                batteryName += " - " + QString::number(victronConnection->batteryModelId());
            }

            ThingClass batteryThingClass = thingClass(victronBatteryThingClassId);
            ThingDescriptor descriptor(victronBatteryThingClassId, batteryName, QString::number(victronConnection->batterySerialNumber()), thing->id());
            // No params required, all we need is the connection
            emit autoThingsAppeared(ThingDescriptors() << descriptor);
        }

        return;
    }

    if (thing->thingClassId() == victronMeterThingClassId || thing->thingClassId() == victronBatteryThingClassId) {
        Thing *connectionThing = myThings().findById(thing->parentId());
        if (connectionThing) {
            thing->setStateValue("connected", connectionThing->stateValue("connected"));
        }

        return;
    }
}

void IntegrationPluginVictron::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == victronInverterThingClassId && m_victronConnections.contains(thing)) {
        VictronModbusTcpConnection *connection = m_victronConnections.take(thing);
        delete connection;
    }

    // Unregister related hardware resources
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginVictron::setupVictronConnection(ThingSetupInfo *info)
{
    Thing *thing = info->thing();

    QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();
    uint port = thing->paramValue(victronInverterThingPortParamTypeId).toUInt();
    quint16 slaveId = thing->paramValue(victronInverterThingSlaveIdParamTypeId).toUInt();

    qCDebug(dcVictron()) << "Setting up victron on" << address.toString() << port << "unit ID:" << slaveId;
    VictronModbusTcpConnection *victronConnection = new VictronModbusTcpConnection(address, port, slaveId, this);
    connect(info, &ThingSetupInfo::aborted, victronConnection, &VictronModbusTcpConnection::deleteLater);

    // Reconnect on monitor reachable changed
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
        qCDebug(dcVictron()) << "Network device monitor reachable changed for" << thing->name() << reachable;
        if (!thing->setupComplete())
            return;

        if (reachable && !thing->stateValue("connected").toBool()) {
            victronConnection->setHostAddress(monitor->networkDeviceInfo().address());
            victronConnection->connectDevice();
        } else if (!reachable) {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            victronConnection->disconnectDevice();
        }
    });

    connect(victronConnection, &VictronModbusTcpConnection::reachableChanged, thing, [this, thing, victronConnection](bool reachable){
        qCDebug(dcVictron()) << "Reachable changed to" << reachable << "for" << thing;
        if (reachable) {
            // Connected true will be set after successfull init
            victronConnection->initialize();
        } else {
            thing->setStateValue("connected", false);
            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", false);
            }
        }
    });

    connect(victronConnection, &VictronModbusTcpConnection::initializationFinished, thing, [=](bool success){
        if (!thing->setupComplete())
            return;

        thing->setStateValue("connected", success);
        foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
            childThing->setStateValue("connected", success);
        }

        if (!success) {
            // Try once to reconnect the device
            victronConnection->reconnectDevice();
        }
    });

    connect(victronConnection, &VictronModbusTcpConnection::initializationFinished, info, [=](bool success){
        if (!success) {
            qCWarning(dcVictron()) << "Connection init finished with errors" << thing->name() << victronConnection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            victronConnection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the inverter."));
            return;
        }

        qCDebug(dcVictron()) << "Connection init finished successfully" << victronConnection;
        m_victronConnections.insert(thing, victronConnection);
        info->finish(Thing::ThingErrorNoError);

        // Set connected true
        thing->setStateValue("connected", true);
        foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
            childThing->setStateValue("connected", true);
        }

        // Make sure we use the same endianness as the inverter (register size 1, so independent from endianess)
        switch (victronConnection->modbusByteOrder()) {
        case VictronModbusTcpConnection::ByteOrderBigEndian:
            victronConnection->setEndianness(ModbusDataUtils::ByteOrderBigEndian);
            break;
        case VictronModbusTcpConnection::ByteOrderLittleEndian:
            victronConnection->setEndianness(ModbusDataUtils::ByteOrderLittleEndian);
            break;
        }

        connect(victronConnection, &VictronModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcVictron()) << "Updated" << victronConnection;

            // Current
            thing->setStateValue(victronInverterPhaseACurrentStateTypeId, victronConnection->currentPhase1());
            thing->setStateValue(victronInverterPhaseBCurrentStateTypeId, victronConnection->currentPhase2());
            thing->setStateValue(victronInverterPhaseCCurrentStateTypeId, victronConnection->currentPhase3());

            // Voltage
            thing->setStateValue(victronInverterVoltagePhaseAStateTypeId, victronConnection->voltagePhase1());
            thing->setStateValue(victronInverterVoltagePhaseBStateTypeId, victronConnection->voltagePhase2());
            thing->setStateValue(victronInverterVoltagePhaseCStateTypeId, victronConnection->voltagePhase3());

            // Phase power
            thing->setStateValue(victronInverterCurrentPowerPhaseAStateTypeId, victronConnection->activePowerPhase1());
            thing->setStateValue(victronInverterCurrentPowerPhaseBStateTypeId, victronConnection->activePowerPhase2());
            thing->setStateValue(victronInverterCurrentPowerPhaseCStateTypeId, victronConnection->activePowerPhase3());

            // Others
            thing->setStateValue(victronInverterFrequencyStateTypeId, victronConnection->gridFrequencyInverter());
            thing->setStateValue(victronInverterTotalEnergyProducedStateTypeId, victronConnection->totalYield() / 1000.0); // kWh

            // Power
            thing->setStateValue(victronInverterCurrentPowerStateTypeId, - (victronConnection->totalAcPower() - victronConnection->batteryActualPower()));

            // Update the battery if available
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(victronBatteryThingClassId);
            if (batteryThings.count() == 1) {
                Thing *batteryThing = batteryThings.first();

                batteryThing->setStateValue(victronBatteryVoltageStateTypeId, victronConnection->batteryVoltage());
                batteryThing->setStateValue(victronBatteryTemperatureStateTypeId, victronConnection->batteryTemperature());
                batteryThing->setStateValue(victronBatteryBatteryLevelStateTypeId, victronConnection->batteryStateOfCharge());
                batteryThing->setStateValue(victronBatteryBatteryCriticalStateTypeId, victronConnection->batteryStateOfCharge() < 5);

                // TODO: Check if this is correct
                batteryThing->setStateValue(victronBatteryCapacityStateTypeId, victronConnection->batteryWorkCapacity() / 1000.0); // kWh

                double batteryPower = -victronConnection->batteryActualPower();
                batteryThing->setStateValue(victronBatteryCurrentPowerStateTypeId, batteryPower);
                if (batteryPower == 0) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "idle");
                } else if (batteryPower < 0) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "discharging");
                } else if (batteryPower > 0) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "charging");
                }
            }

            // Update the meter if available
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(victronMeterThingClassId);
            if (meterThings.count() == 1) {
                Thing *meterThing = meterThings.first();

                // Power
                meterThing->setStateValue(victronMeterCurrentPhaseAStateTypeId, victronConnection->powerMeterCurrentPhase1());
                meterThing->setStateValue(victronMeterCurrentPhaseBStateTypeId, victronConnection->powerMeterCurrentPhase2());
                meterThing->setStateValue(victronMeterCurrentPhaseCStateTypeId, victronConnection->powerMeterCurrentPhase3());

                // Voltage
                meterThing->setStateValue(victronMeterVoltagePhaseAStateTypeId, victronConnection->powerMeterVoltagePhase1());
                meterThing->setStateValue(victronMeterVoltagePhaseBStateTypeId, victronConnection->powerMeterVoltagePhase2());
                meterThing->setStateValue(victronMeterVoltagePhaseCStateTypeId, victronConnection->powerMeterVoltagePhase3());

                // Current
                meterThing->setStateValue(victronMeterCurrentPowerPhaseAStateTypeId, victronConnection->powerMeterActivePowerPhase1());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseBStateTypeId, victronConnection->powerMeterActivePowerPhase2());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseCStateTypeId, victronConnection->powerMeterActivePowerPhase3());

                meterThing->setStateValue(victronMeterFrequencyStateTypeId, victronConnection->gridFrequencyPowerMeter());

                meterThing->setStateValue(victronMeterTotalEnergyConsumedStateTypeId, victronConnection->totalHomeConsumptionFromGrid() / 1000.0); // kWh
                meterThing->setStateValue(victronMeterTotalEnergyProducedStateTypeId, victronConnection->totalEnergyAcToGrid() / 1000.0); // kWh

                // Set the power as last value
                meterThing->setStateValue(victronMeterCurrentPowerStateTypeId, victronConnection->powerMeterTotalActivePower());
            }
        });

        // Update registers
        victronConnection->update();
    });

    victronConnection->connectDevice();
}
