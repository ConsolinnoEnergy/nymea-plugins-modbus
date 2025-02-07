/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2025, nymea GmbH
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
    VictronDiscovery *discovery = new VictronDiscovery(hardwareManager()->networkDeviceDiscovery(), m_modbusTcpPort, m_modbusSlaveAddress, info);
    connect(discovery, &VictronDiscovery::discoveryFinished, discovery, &VictronDiscovery::deleteLater);
    connect(discovery, &VictronDiscovery::discoveryFinished, info, [=](){
        foreach (const VictronDiscovery::VictronDiscoveryResult &result, discovery->discoveryResults()) {
            QString title = "Victron Inverter";
            ThingDescriptor descriptor(victronInverterTcpThingClassId, title, " " + result.networkDeviceInfo.address().toString() + " " + result.networkDeviceInfo.macAddress()); //JoOb: to be adapted?
            qCDebug(dcVictron()) << "Discovered:" << descriptor.title() << descriptor.description(); 

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(victronInverterTcpThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcVictron()) << "This Victron inverter already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(victronInverterTcpThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
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

    if (thing->thingClassId() == victronInverterTcpThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcVictron()) << "Reconfiguring existing thing" << thing->name();
            m_tcpConnections.take(thing)->deleteLater();

            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }

        MacAddress macAddress = MacAddress(thing->paramValue(victronInverterTcpThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcVictron()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the thing."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);        
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Clean up in case the setup gets aborted
            if (m_monitors.contains(thing)) {
                qCDebug(dcVictron()) << "Unregister monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();

        qCInfo(dcVictron()) << "Setting up Victron on" << address.toString();
        auto victronConnection = new VictronModbusTcpConnection(address, m_modbusTcpPort , m_modbusSlaveAddress, this);
        connect(info, &ThingSetupInfo::aborted, victronConnection, &VictronModbusTcpConnection::deleteLater);

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcVictron()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->setupComplete())
                return;

            if (reachable && !thing->stateValue("connected").toBool()) {
                victronConnection->setHostAddress(monitor->networkDeviceInfo().address());
                victronConnection->reconnectDevice();
            } else if (!reachable) {
                // Note: Auto reconnect is disabled explicitly and
                // the device will be connected once the monitor says it is reachable again
                victronConnection->disconnectDevice();
            }
        });

        connect(victronConnection, &VictronModbusTcpConnection::reachableChanged, thing, [this, thing, victronConnection](bool reachable){
            qCInfo(dcVictron()) << "Reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                victronConnection->initialize();
            } else {
                thing->setStateValue("connected", false);
                thing->setStateValue(victronInverterTcpCurrentPowerStateTypeId, 0);

                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }

                Thing *child = getMeterThing(thing);
                if (child) {
                    child->setStateValue(victronMeterCurrentPowerStateTypeId, 0);
                    child->setStateValue(victronMeterCurrentPhaseAStateTypeId, 0);
                    child->setStateValue(victronMeterCurrentPhaseBStateTypeId, 0);
                    child->setStateValue(victronMeterCurrentPhaseCStateTypeId, 0);
                }

                child = getBatteryThing(thing);
                if (child) {
                    child->setStateValue(victronBatteryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(victronConnection, &VictronModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue("connected", success);

            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", success);
            }

            if (!success) {
                // Try once to reconnect the device
                victronConnection->reconnectDevice();
            } else {
                qCInfo(dcVictron()) << "Connection initialized successfully for" << thing;
                victronConnection->update();
            }
        });

        connect(victronConnection, &VictronModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcVictron()) << "Updated" << victronConnection;

            if (myThings().filterByParentId(thing->id()).filterByThingClassId(victronMeterThingClassId).isEmpty()) {
                qCDebug(dcVictron()) << "There is no meter set up for this inverter. Creating a meter for" << thing << victronConnection;
                ThingClass meterThingClass = thingClass(victronMeterThingClassId);
                ThingDescriptor descriptor(victronMeterThingClassId, meterThingClass.displayName() + " " + victronConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Check if a battery is connected to this Victron inverter
            if (victronConnection->batteryVoltage() != 0 &&
                    myThings().filterByParentId(thing->id()).filterByThingClassId(victronBatteryThingClassId).isEmpty()) {
                qCDebug(dcVictron()) << "There is a battery connected but not set up yet. Creating a battery.";
                ThingClass batteryThingClass = thingClass(victronBatteryThingClassId);
                ThingDescriptor descriptor(victronBatteryThingClassId, batteryThingClass.displayName() + " " + victronConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Update inverter states
            double powerPV=victronConnection->inverterPowerPvOutA()+victronConnection->inverterPowerPvOutB()+victronConnection->inverterPowerPvOutC()\
                +victronConnection->inverterPowerPvInpA()+victronConnection->inverterPowerPvInpB()+victronConnection->inverterPowerPvInpC();
            thing->setStateValue(victronInverterTcpCurrentPowerStateTypeId, powerPV * -1);
            // thing->setStateValue(victronInverterTcpTemperatureStateTypeId, victronConnection->inverterTemperature());
            thing->setStateValue(victronInverterTcpFrequencyStateTypeId, victronConnection->meterFrequency());
            // thing->setStateValue(victronInverterTcpTotalEnergyProducedStateTypeId, victronConnection->totalPVGeneration());

            // Update the meter if available
            Thing *meterThing = getMeterThing(thing);
            if (meterThing) {
                // auto runningState = victronConnection->runningState();
                // qCDebug(dcVictron()) << "Power generated from PV:" << (runningState & (0x1 << 0) ? "true" : "false");
                // qCDebug(dcVictron()) << "Battery charging:" << (runningState & (0x1 << 1) ? "true" : "false");
                // qCDebug(dcVictron()) << "Battery discharging:" << (runningState & (0x1 << 2) ? "true" : "false");
                // qCDebug(dcVictron()) << "Positive load power:" << (runningState & (0x1 << 3) ? "true" : "false");
                // qCDebug(dcVictron()) << "Feed-in power:" << (runningState & (0x1 << 4) ? "true" : "false");
                // qCDebug(dcVictron()) << "Import power from grid:" << (runningState & (0x1 << 5) ? "true" : "false");
                // qCDebug(dcVictron()) << "Negative load power:" << (runningState & (0x1 << 7) ? "true" : "false");
                double meterPower = victronConnection->meterPowerA()+victronConnection->meterPowerB()+victronConnection->meterPowerC();
                meterThing->setStateValue(victronMeterCurrentPowerStateTypeId, meterPower);                
                meterThing->setStateValue(victronMeterTotalEnergyConsumedStateTypeId, victronConnection->meterTotalEnergyConsumed());
                meterThing->setStateValue(victronMeterTotalEnergyProducedStateTypeId, victronConnection->meterTotalEnergyProduced());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseAStateTypeId, victronConnection->meterPowerA());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseBStateTypeId, victronConnection->meterPowerB());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseCStateTypeId, victronConnection->meterPowerC());
                meterThing->setStateValue(victronMeterCurrentPhaseAStateTypeId, victronConnection->meterCurrentA());
                meterThing->setStateValue(victronMeterCurrentPhaseBStateTypeId, victronConnection->meterCurrentB());
                meterThing->setStateValue(victronMeterCurrentPhaseCStateTypeId, victronConnection->meterCurrentC());
                meterThing->setStateValue(victronMeterVoltagePhaseAStateTypeId, victronConnection->meterVoltageA());
                meterThing->setStateValue(victronMeterVoltagePhaseBStateTypeId, victronConnection->meterVoltageB());
                meterThing->setStateValue(victronMeterVoltagePhaseCStateTypeId, victronConnection->meterVoltageB());
                meterThing->setStateValue(victronMeterFrequencyStateTypeId, victronConnection->meterFrequency());
            }

            // Update the battery if available
            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                batteryThing->setStateValue(victronBatteryVoltageStateTypeId, victronConnection->batteryVoltage());
                batteryThing->setStateValue(victronBatteryTemperatureStateTypeId, victronConnection->batteryTemperature());
                batteryThing->setStateValue(victronBatteryBatteryLevelStateTypeId, victronConnection->batterySoc());
                batteryThing->setStateValue(victronBatteryBatteryCriticalStateTypeId, victronConnection->batterySoc() < 5);
                // batteryThing->setStateValue(victronBatteryCapacityStateTypeId, victronConnection->totalBatteryCapacity());

                // quint16 runningState = victronConnection->runningState();
                double batteryPower = static_cast<double>(victronConnection->batteryPower());
                if (victronConnection->batteryState()==1) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "charging");
                } else if(victronConnection->batteryState()==2) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "discharging");
                } else {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "idle");
                }
                batteryThing->setStateValue(victronBatteryCurrentPowerStateTypeId, batteryPower);
            }
        });

        m_tcpConnections.insert(thing, victronConnection);

        if (monitor->reachable())
            victronConnection->connectDevice();

        info->finish(Thing::ThingErrorNoError);
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

        VictronModbusTcpConnection *victronConnection = m_tcpConnections.value(connectionThing);
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

        VictronModbusTcpConnection *victronConnection = m_tcpConnections.value(connectionThing);
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
    if (thing->thingClassId() == victronInverterTcpThingClassId) {

        // Create the update timer if not already set up
        if (!m_refreshTimer) {
            qCDebug(dcVictron()) << "Starting plugin timer...";
            m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
            connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
                foreach(auto thing, myThings().filterByThingClassId(victronInverterTcpThingClassId)) {
                    auto monitor = m_monitors.value(thing);
                    if (!monitor->reachable()) {
                        continue;
                    }

                    auto connection = m_tcpConnections.value(thing);

                    if (connection->reachable()) {
                        qCDebug(dcVictron()) << "Updating connection" << connection->hostAddress().toString();
                        connection->update();
                    } else {
                        qCDebug(dcVictron()) << "Device not reachable. Probably a TCP connection error. Reconnecting TCP socket";
                        connection->reconnectDevice();
                    }
                }
            });
            m_refreshTimer->start();
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
    if (thing->thingClassId() == victronInverterTcpThingClassId && m_tcpConnections.contains(thing)) {
        auto connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    // Unregister related hardware resources
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcVictron()) << "Stopping refresh timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

Thing *IntegrationPluginVictron::getMeterThing(Thing *parentThing)
{
    Things meterThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(victronMeterThingClassId);
    if (meterThings.isEmpty())
        return nullptr;

    return meterThings.first();
}

Thing *IntegrationPluginVictron::getBatteryThing(Thing *parentThing)
{
    Things batteryThings = myThings().filterByParentId(parentThing->id()).filterByThingClassId(victronBatteryThingClassId);
    if (batteryThings.isEmpty())
        return nullptr;

    return batteryThings.first();
}
