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
    VictronDiscovery *discovery = new VictronDiscovery(hardwareManager()->networkDeviceDiscovery(), m_modbusTcpPort, m_systemModbusSlaveAddress, info);
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
            // FIXME: UnitID Params entered by user also needed here?
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
        if (m_systemTcpConnections.contains(thing)) {
            qCDebug(dcVictron()) << "System: Reconfiguring existing thing" << thing->name();
            m_systemTcpConnections.take(thing)->deleteLater();

            if (m_monitors.contains(thing)) {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        }
        if (m_vebusTcpConnections.contains(thing)) {
            qCDebug(dcVictron()) << "Vebus: Reconfiguring existing thing" << thing->name();
            m_vebusTcpConnections.take(thing)->deleteLater();
        }
        if (m_gridTcpConnections.contains(thing)) {
            qCDebug(dcVictron()) << "Grid: Reconfiguring existing thing" << thing->name();
            m_gridTcpConnections.take(thing)->deleteLater();
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
        quint16 systemSlaveAddress = thing->paramValue(victronInverterTcpThingUnitIdSystemParamTypeId).toInt();
        quint16 vebusSlaveAddress = thing->paramValue(victronInverterTcpThingUnitIdVebusParamTypeId).toInt();
        quint16 gridSlaveAddress = thing->paramValue(victronInverterTcpThingUnitIdGridParamTypeId).toInt();


        qCInfo(dcVictron()) << "Setting up Victron on" << address.toString();
        auto systemConnection = new VictronSystemModbusTcpConnection(address, m_modbusTcpPort , systemSlaveAddress, this);
        connect(info, &ThingSetupInfo::aborted, systemConnection, &VictronSystemModbusTcpConnection::deleteLater);

        auto vebusConnection = new VictronVebusModbusTcpConnection(address, m_modbusTcpPort , vebusSlaveAddress, this); //JoOb
        connect(info, &ThingSetupInfo::aborted, vebusConnection, &VictronVebusModbusTcpConnection::deleteLater); // JoOb

        auto gridConnection = new VictronGridModbusTcpConnection(address, m_modbusTcpPort , gridSlaveAddress, this); //JoOb
        connect(info, &ThingSetupInfo::aborted, gridConnection, &VictronGridModbusTcpConnection::deleteLater); // JoOb


        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcVictron()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->setupComplete())
                return;

            if (reachable && !thing->stateValue("connected").toBool()) {
                systemConnection->setHostAddress(monitor->networkDeviceInfo().address());
                systemConnection->reconnectDevice();
                vebusConnection->setHostAddress(monitor->networkDeviceInfo().address());
                vebusConnection->reconnectDevice();
                gridConnection->setHostAddress(monitor->networkDeviceInfo().address());
                gridConnection->reconnectDevice();
            } else if (!reachable) {
                // Note: Auto reconnect is disabled explicitly and
                // the device will be connected once the monitor says it is reachable again
                systemConnection->disconnectDevice();
                vebusConnection->disconnectDevice();
                gridConnection->disconnectDevice();
            }
        });

        connect(systemConnection, &VictronSystemModbusTcpConnection::reachableChanged, thing, [this, thing, systemConnection](bool reachable){
            qCInfo(dcVictron()) << "System reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                systemConnection->initialize();
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

        connect(vebusConnection, &VictronVebusModbusTcpConnection::reachableChanged, thing, [this, thing, vebusConnection](bool reachable){
            qCInfo(dcVictron()) << "Vebus reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                vebusConnection->initialize();
            }
        });

        connect(gridConnection, &VictronGridModbusTcpConnection::reachableChanged, thing, [this, thing, gridConnection](bool reachable){
            qCInfo(dcVictron()) << "Grid reachable changed to" << reachable << "for" << thing;
            if (reachable) {
                // Connected true will be set after successfull init
                gridConnection->initialize();
            }
        });

        connect(systemConnection, &VictronSystemModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue("connected", success);

            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", success);
            }

            if (!success) {
                // Try once to reconnect the device
                systemConnection->reconnectDevice();
            } else {
                qCInfo(dcVictron()) << "System: Connection initialized successfully for" << thing;
                systemConnection->update();
            }
        });

        connect(vebusConnection, &VictronVebusModbusTcpConnection::initializationFinished, thing, [=](bool success){
            if (!success) {
                // Try once to reconnect the device
                vebusConnection->reconnectDevice();
            } else {
                qCInfo(dcVictron()) << "Vebus: Connection initialized successfully for" << thing;
            }
        });

        connect(gridConnection, &VictronGridModbusTcpConnection::initializationFinished, thing, [=](bool success){
            if (!success) {
                // Try once to reconnect the device
                gridConnection->reconnectDevice();
            } else {
                qCInfo(dcVictron()) << "Grid: Connection initialized successfully for" << thing;
            }
        });

        connect(systemConnection, &VictronSystemModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcVictron()) << "Updated" << systemConnection;

            if (myThings().filterByParentId(thing->id()).filterByThingClassId(victronMeterThingClassId).isEmpty()) {
                qCDebug(dcVictron()) << "There is no meter set up for this inverter. Creating a meter for" << thing << systemConnection;
                ThingClass meterThingClass = thingClass(victronMeterThingClassId);
                ThingDescriptor descriptor(victronMeterThingClassId, meterThingClass.displayName() + " " + systemConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Check if a battery is connected to this Victron inverter
            if (systemConnection->batteryVoltage() != 0 &&
                    myThings().filterByParentId(thing->id()).filterByThingClassId(victronBatteryThingClassId).isEmpty()) {
                qCDebug(dcVictron()) << "There is a battery connected but not set up yet. Creating a battery.";
                ThingClass batteryThingClass = thingClass(victronBatteryThingClassId);
                ThingDescriptor descriptor(victronBatteryThingClassId, batteryThingClass.displayName() + " " + systemConnection->serialNumber(), QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            // Update inverter states
            double powerPV=systemConnection->inverterPowerPvOutA()+systemConnection->inverterPowerPvOutB()+systemConnection->inverterPowerPvOutC()\
                +systemConnection->inverterPowerPvInpA()+systemConnection->inverterPowerPvInpB()+systemConnection->inverterPowerPvInpC()+systemConnection->powerPvDc();
            thing->setStateValue(victronInverterTcpCurrentPowerStateTypeId, powerPV * -1);
            // thing->setStateValue(victronInverterTcpTemperatureStateTypeId, systemConnection->inverterTemperature());
            // thing->setStateValue(victronInverterTcpTotalEnergyProducedStateTypeId, systemConnection->totalPVGeneration());

            // Update the meter if available
            Thing *meterThing = getMeterThing(thing);
            if (meterThing) {
                double meterPower = systemConnection->meterPowerA()+systemConnection->meterPowerB()+systemConnection->meterPowerC();
                meterThing->setStateValue(victronMeterCurrentPowerStateTypeId, meterPower);
                meterThing->setStateValue(victronMeterCurrentPowerPhaseAStateTypeId, systemConnection->meterPowerA());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseBStateTypeId, systemConnection->meterPowerB());
                meterThing->setStateValue(victronMeterCurrentPowerPhaseCStateTypeId, systemConnection->meterPowerC());
            }

            // Update the battery if available
            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                batteryThing->setStateValue(victronBatteryEnableForcePowerStateStateTypeId, systemConnection->essMode()==3);
                
                batteryThing->setStateValue(victronBatteryVoltageStateTypeId, systemConnection->batteryVoltage());
                // batteryThing->setStateValue(victronBatteryTemperatureStateTypeId, systemConnection->batteryTemperature());
                batteryThing->setStateValue(victronBatteryBatteryLevelStateTypeId, systemConnection->batterySoc());
                batteryThing->setStateValue(victronBatteryBatteryCriticalStateTypeId, systemConnection->batterySoc() < 5);
                // batteryThing->setStateValue(victronBatteryCapacityStateTypeId, systemConnection->totalBatteryCapacity());

                // quint16 runningState = systemConnection->runningState();
                double batteryPower = static_cast<double>(systemConnection->batteryPower());
                if (systemConnection->batteryState()==1) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "charging");
                } else if(systemConnection->batteryState()==2) {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "discharging");
                } else {
                    batteryThing->setStateValue(victronBatteryChargingStateStateTypeId, "idle");
                }
                batteryThing->setStateValue(victronBatteryCurrentPowerStateTypeId, batteryPower);
            }
        });

        connect(vebusConnection, &VictronVebusModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcVictron()) << "Updated" << vebusConnection;
            // update inverter values
            thing->setStateValue(victronInverterTcpFrequencyStateTypeId, vebusConnection->inverterOutputFrequency());

            // writing setpoints if battery exists
            Thing *batteryThing = getBatteryThing(thing);
            if (batteryThing) {
                bool state = batteryThing->stateValue(victronBatteryEnableForcePowerStateStateTypeId).toBool();
                float powerToSet = batteryThing->stateValue(victronBatteryForcePowerStateTypeId).toFloat();
                float powerInvOut = vebusConnection->inverterPowerOutputPhaseA()\
                    +vebusConnection->inverterPowerOutputPhaseB()+vebusConnection->inverterPowerOutputPhaseC();
                int16_t powerSetPhase = static_cast<int16_t>((powerToSet+powerInvOut)/3); // Victron control variable is the AC input power of the inverter, not the battery power
                qCDebug(dcVictron()) << "State " <<  state << "; powerSetPhase " << powerSetPhase;
                
                // Write battery power cyclic if remote control is enabled
                if (systemConnection->essMode()==3 && m_setpointTimer==0){
                    vebusConnection->writeSetpoints(powerSetPhase);
                }
            }         
        });

        connect(gridConnection, &VictronGridModbusTcpConnection::updateFinished, thing, [=](){
            qCDebug(dcVictron()) << "Updated" << gridConnection;
            // update grid values
            Thing *meterThing = getMeterThing(thing);
            if (meterThing) {
                meterThing->setStateValue(victronMeterFrequencyStateTypeId, gridConnection->gridFrequency());                
                meterThing->setStateValue(victronMeterTotalEnergyConsumedStateTypeId, 0.01*gridConnection->gridTotalEnergyConsumed());
                meterThing->setStateValue(victronMeterTotalEnergyProducedStateTypeId, 0.01*gridConnection->gridTotalEnergyProduced());                
                meterThing->setStateValue(victronMeterCurrentPhaseAStateTypeId, gridConnection->gridCurrentPhaseA());
                meterThing->setStateValue(victronMeterCurrentPhaseBStateTypeId, gridConnection->gridCurrentPhaseB());
                meterThing->setStateValue(victronMeterCurrentPhaseCStateTypeId, gridConnection->gridCurrentPhaseC());
                meterThing->setStateValue(victronMeterVoltagePhaseAStateTypeId, gridConnection->gridVoltagePhaseA());
                meterThing->setStateValue(victronMeterVoltagePhaseBStateTypeId, gridConnection->gridVoltagePhaseB());
                meterThing->setStateValue(victronMeterVoltagePhaseCStateTypeId, gridConnection->gridVoltagePhaseC());
                meterThing->setStateValue(victronMeterFrequencyStateTypeId, gridConnection->gridFrequency());
            }               
        });
        m_systemTcpConnections.insert(thing, systemConnection);
        m_vebusTcpConnections.insert(thing, vebusConnection); //JoOb
        m_gridTcpConnections.insert(thing, gridConnection); //JoOb
        
        if (monitor->reachable())
            systemConnection->connectDevice();

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

        VictronSystemModbusTcpConnection *systemConnection = m_systemTcpConnections.value(connectionThing);
        if (!systemConnection) {
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

        VictronSystemModbusTcpConnection *systemConnection = m_systemTcpConnections.value(connectionThing);
        if (!systemConnection) {
            qCWarning(dcVictron()) << "Failed to set up victron battery because the connection for" << connectionThing << "does not exist.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Note: The states will be handled in the parent inverter thing on updated

        info->finish(Thing::ThingErrorNoError);

        // Set battery capacity from settings on restart.
        thing->setStateValue(victronBatteryCapacityStateTypeId, thing->setting(victronBatterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == victronBatterySettingsCapacityParamTypeId) {
                qCDebug(dcVictron()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(victronBatteryCapacityStateTypeId, value.toInt());
            }
        });
        
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
                // Timer in seconds for writing setpoints, current calibration every 10sec
                m_setpointTimer += 5;
                if (m_setpointTimer > 5) {
                    m_setpointTimer = 0;                    
                }
                
                foreach(auto thing, myThings().filterByThingClassId(victronInverterTcpThingClassId)) {
                    auto monitor = m_monitors.value(thing);
                    if (!monitor->reachable()) {
                        continue;
                    }

                    auto connection = m_systemTcpConnections.value(thing);

                    if (connection->reachable()) {
                        qCDebug(dcVictron()) << "System: Updating connection" << connection->hostAddress().toString();
                        connection->update();
                    }

                    // triggering multiple updates at the same time should be fine due to different slave IDs
                    auto vebusConnection = m_vebusTcpConnections.value(thing);

                    if (vebusConnection->reachable()) {
                        qCDebug(dcVictron()) << "Vebus: Updating connection" << vebusConnection->hostAddress().toString();
                        vebusConnection->update();
                    }

                    // triggering multiple updates at the same time should be fine due to different slave IDs
                    auto gridConnection = m_gridTcpConnections.value(thing);

                    if (gridConnection->reachable()) {
                        qCDebug(dcVictron()) << "Grid: Updating connection" << gridConnection->hostAddress().toString();
                        gridConnection->update();
                    }
                }
            });
            m_setpointTimer = 0;
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

// Code for setting power limit. Disabled for now.
void IntegrationPluginVictron::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == victronBatteryThingClassId) {
        Thing *inverterThing = myThings().findById(thing->parentId());
        qCWarning(dcVictron()) << "executeAction: should be inverter thing" << inverterThing;

        if (action.actionTypeId() == victronBatteryEnableForcePowerActionTypeId) {
            bool state = action.paramValue(victronBatteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            qCWarning(dcVictron()) << "Enable Battery manual mode should be set to" << state;
            thing->setStateValue(victronBatteryEnableForcePowerStateTypeId, state);

            activateRemoteControl(inverterThing, state);
        } else if (action.actionTypeId() == victronBatteryForcePowerActionTypeId) {
            int batteryPower = action.paramValue(victronBatteryForcePowerActionForcePowerParamTypeId).toInt();
            qCWarning(dcVictron()) << "Battery power should be set to" << batteryPower;
            thing->setStateValue(victronBatteryForcePowerStateTypeId, batteryPower);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    }
}

void IntegrationPluginVictron::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == victronInverterTcpThingClassId) {
        if (m_systemTcpConnections.contains(thing)) {
            auto connection = m_systemTcpConnections.take(thing);
            connection->disconnectDevice();
            delete connection;
        }

        if (m_vebusTcpConnections.contains(thing)) {
            auto connection = m_vebusTcpConnections.take(thing);
            connection->disconnectDevice();
            delete connection;
        }

        if (m_gridTcpConnections.contains(thing)) {
            auto connection = m_gridTcpConnections.take(thing);
            connection->disconnectDevice();
            delete connection;
        }
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

void IntegrationPluginVictron::activateRemoteControl(Thing *thing, bool activation)
{
    if (thing->thingClassId() == victronInverterTcpThingClassId) {
        VictronSystemModbusTcpConnection *systemConnection = m_systemTcpConnections.value(thing);
        
        if (!systemConnection) {
            qCWarning(dcVictron()) << "setBatteryPower - Victron GX Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        // one time writing of ESS mode (register 2902)
        quint16 essMode = (activation) ?3 : 1;
        QModbusReply *replyMode = systemConnection->setEssMode(essMode);
        connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
        connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode](){
            if (replyMode->error() != QModbusDevice::NoError) {
                qCWarning(dcVictron()) << "Remote control: Error during setting" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcVictron()) << "Remote control: Successfully set";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
    } else {
        qCWarning(dcVictron()) << "Remote control: Received wrong thing";
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
