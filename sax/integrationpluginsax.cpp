/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "integrationpluginsax.h"
#include "plugininfo.h"

#include "saxstoragediscovery.h"

#include "saxmodbustcpconnection.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>
#include <plugintimer.h>

IntegrationPluginSax::IntegrationPluginSax()
{

}

void IntegrationPluginSax::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == saxStorageThingClassId) {
        SaxStorageDiscovery *discovery = new SaxStorageDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &SaxStorageDiscovery::discoveryFinished, info, [this, info, discovery](){
            qCInfo(dcSax()) << "Discovery results:" << discovery->discoveryResults().count();
            ThingClass thingClass = supportedThings().findById(info->thingClassId());            

            foreach (const SaxStorageDiscovery::Result &result, discovery->discoveryResults()) {
                qCDebug(dcSax()) << "Discovery result:" << result.networkDeviceInfo.address().toString() + " (" + result.networkDeviceInfo.macAddress() + ", " + result.networkDeviceInfo.macAddressManufacturer() + ")";

                //draft: check if found devices have a valid capacity register
                if (result.capacity_register == 5200 || result.capacity_register == 10400 || result.capacity_register == 15600){   
                    qCDebug(dcSax()) << "Discovery: --> Found Version with capacity:" 
                                        << result.capacity_register << "Wh";                      
                }
                else{
                    qCDebug(dcSax()) << "Discovery: --> Found wrong Version, no valid capacity register (" 
                                        << result.capacity_register << ")";  
                    // continue;
                }

                ThingDescriptor descriptor(info->thingClassId(), thingClass.displayName(), result.networkDeviceInfo.address().toString());
                
                ParamList params{
                    {saxStorageThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress()}
                };
                descriptor.setParams(params);

                Thing *existingThing = myThings().findByParams(params);
                if (existingThing) {
                    descriptor.setThingId(existingThing->id());
                }
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);

        });
        discovery->startDiscovery();

    }
}

void IntegrationPluginSax::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSax()) << "Setup" << thing;

    if (thing->thingClassId() == saxStorageThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSax()) << "Already have a Sax connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcSax()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(saxStorageThingMacAddressParamTypeId).toString());
        if (!macAddress.isValid()) {
            qCWarning(dcSax()) << "Failed to set up Sax Battery because the MAC address is not valid:" << thing->paramValue(saxStorageThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        qint16 port = MODBUS_PORT;
        qint16 slaveId = SLAVE_ID;


        SaxModbusTcpConnection *connection = new SaxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        connect(info, &ThingSetupInfo::aborted, connection, &SaxModbusTcpConnection::deleteLater);

        connect(connection, &SaxModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
            qCDebug(dcSax()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(saxStorageConnectedStateTypeId, false);
                // thing->setStateValue(saxMeterConnectedStateTypeId, false);
            }
        });

        connect(connection, &SaxModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcSax()) << "Sax initialized.";
                m_tcpConnections.insert(thing, connection);
                m_monitors.insert(thing, monitor);
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                connection->deleteLater();
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the battery."));
            }
        });

        connect(connection, &SaxModbusTcpConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue(saxStorageConnectedStateTypeId, true);
                // thing->setStateValue(saxMeterConnectedStateTypeId, true);
                connection->update();
            }
        });



        // Handle property changed signals

        /*Debug message for update message*/
        connect(connection, &SaxModbusTcpConnection::updateFinished, thing, [connection, thing](){
            qCDebug(dcSax()) << "Updated:" << connection;
        });

        /*current Power battery*/
        connect(connection, &SaxModbusTcpConnection::powerBatteryChanged, thing, [connection, thing](qint16 currentPower){
            qint16 powerfactor = connection->powerFactorBattery();
            double powerConverted = currentPower * qPow(10, powerfactor);

            qCDebug(dcSax()) << "Battery power changed" << powerConverted << "W";

            /*battery charging state*/
            /*Information from Sax: positive power means discharging*/
            if (powerConverted > 0) {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "discharging");
            } else if (powerConverted < 0) {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "charging");
            } else {
                thing->setStateValue(saxStorageChargingStateStateTypeId, "idle");
            }
            /*In nymea positive power means charging*/
            thing->setStateValue(saxStorageCurrentPowerStateTypeId, -1.0*powerConverted);            
        });

        /*battery current sum [A]*/
        connect(connection, &SaxModbusTcpConnection::currentBatteryChanged, thing, [connection, thing](quint16 current){
            qint16 currentfactor = connection->currentFactorBattery();
            double currentConverted = current * qPow(10, currentfactor);            
            qCDebug(dcSax()) << "Battery current (sum) changed" << currentConverted << "A";
            thing->setStateValue(saxStorageCurrentStateTypeId, currentConverted);
        });

        /*battery current phase A [A]*/
        connect(connection, &SaxModbusTcpConnection::currentBatteryPhaseAChanged, thing, [connection, thing](quint16 current){
            qint16 currentfactor = connection->currentFactorBattery();
            double currentConverted = current * qPow(10, currentfactor);            
            qCDebug(dcSax()) << "Battery current phase A changed" << currentConverted << "A";
            thing->setStateValue(saxStorageCurrentPhaseABatteryStateTypeId, currentConverted);
        });

        /*battery current phase B [A]*/
        connect(connection, &SaxModbusTcpConnection::currentBatteryPhaseBChanged, thing, [connection, thing](quint16 current){
            qint16 currentfactor = connection->currentFactorBattery();
            double currentConverted = current * qPow(10, currentfactor);            
            qCDebug(dcSax()) << "Battery current phase B changed" << currentConverted << "A";
            thing->setStateValue(saxStorageCurrentPhaseBBatteryStateTypeId, currentConverted);
        });

        /*battery current phase C [A]*/
        connect(connection, &SaxModbusTcpConnection::currentBatteryPhaseCChanged, thing, [connection, thing](quint16 current){
            qint16 currentfactor = connection->currentFactorBattery();
            double currentConverted = current * qPow(10, currentfactor);            
            qCDebug(dcSax()) << "Battery current phase C changed" << currentConverted << "A";
            thing->setStateValue(saxStorageCurrentPhaseCBatteryStateTypeId, currentConverted);
        });


        /*battery voltage phase A [V]*/
        connect(connection, &SaxModbusTcpConnection::voltagePhaseABatteryChanged, thing, [connection, thing](quint16 voltage){
            qint16 voltagefactor = connection->voltageFactorBattery();
            double voltageConverted = voltage * qPow(10, voltagefactor);
            qCDebug(dcSax()) << "Battery voltage phase A changed" << voltageConverted << "V";
            thing->setStateValue(saxStorageVoltagePhaseABatteryStateTypeId, voltageConverted);
        });

        /*battery voltage phase B [V]*/
        connect(connection, &SaxModbusTcpConnection::voltagePhaseBBatteryChanged, thing, [connection, thing](quint16 voltage){
            qint16 voltagefactor = connection->voltageFactorBattery();
            double voltageConverted = voltage * qPow(10, voltagefactor);
            qCDebug(dcSax()) << "Battery voltage phase B changed" << voltageConverted << "V";
            thing->setStateValue(saxStorageVoltagePhaseBBatteryStateTypeId, voltageConverted);
        });

        /*battery voltage phase C [V]*/
        connect(connection, &SaxModbusTcpConnection::voltagePhaseCBatteryChanged, thing, [connection, thing](quint16 voltage){
            qint16 voltagefactor = connection->voltageFactorBattery();
            double voltageConverted = voltage * qPow(10, voltagefactor);
            qCDebug(dcSax()) << "Battery voltage phase C changed" << voltageConverted << "V";
            thing->setStateValue(saxStorageVoltagePhaseCBatteryStateTypeId, voltageConverted);
        });

        /*smartmeter frequency*/
        connect(connection, &SaxModbusTcpConnection::frequencyChanged, thing, [this, connection, thing](quint16 frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qint16 frequencyfactor = connection->frequencyFactor();
                double frequencyConverted = frequency * qPow(10, frequencyfactor);

                qCDebug(dcSax()) << "Smartmeter frequency changed" << frequencyConverted << "Hz";
                meterThings.first()->setStateValue(saxMeterFrequencyStateTypeId, frequencyConverted);
            }
        });

        /*battery state of health*/
        connect(connection, &SaxModbusTcpConnection::stateOfHealthChanged, thing, [thing](quint16 stateOfHealth){
            qCDebug(dcSax()) << "Battery stateOfHealth changed" << stateOfHealth << "%";
            thing->setStateValue(saxStorageStateOfHealthStateTypeId, stateOfHealth);
        });


        // /*smartmeter total energy produced*/
        // connect(connection, &SaxModbusTcpConnection::totalEnergyProducedChanged, thing, [connection, thing](quint16 totalEnergyProduced){
        //     qint16 energyfactor = connection->energyFactor();
        //     double energyConverted = totalEnergyProduced * qPow(10, energyfactor);

        //     qCDebug(dcSax()) << "Smartmeter totalEnergyProduced changed" << energyConverted;
        //     thing->setStateValue(saxMeterTotalEnergyProducedStateTypeId, energyConverted);
        // });

        // /*smartmeter total energy consumed*/
        // connect(connection, &SaxModbusTcpConnection::totalEnergyConsumedChanged, thing, [connection, thing](quint16 totalEnergyConsumed){
        //     qint16 energyfactor = connection->energyFactor();
        //     double energyConverted = totalEnergyConsumed * qPow(10, energyfactor);

        //     qCDebug(dcSax()) << "Smartmeter totalEnergyConsumed changed" << energyConverted;
        //     thing->setStateValue(saxMeterTotalEnergyConsumedStateTypeId, energyConverted);
        // });

        /*battery state*/
        connect(connection, &SaxModbusTcpConnection::stateBatteryChanged, thing, [thing](SaxModbusTcpConnection::StateBattery stateBattery){
            qCDebug(dcSax()) << "Battery stateBattery changed" << stateBattery;
            switch (stateBattery) {
                case SaxModbusTcpConnection::StateBatteryOn:
                    thing->setStateValue(saxStorageStateBatteryStateTypeId, "On");
                    break;
                case SaxModbusTcpConnection::StateBatteryStandby:
                    thing->setStateValue(saxStorageStateBatteryStateTypeId, "Standby");
                    break;
                case SaxModbusTcpConnection::StateBatteryOff:
                    thing->setStateValue(saxStorageStateBatteryStateTypeId, "Off");
                    break;
                default:
                    thing->setStateValue(saxStorageStateBatteryStateTypeId, "Off");
                    break;
            }
        });

        // /*smartmeter current phase A*/
        // connect(connection, &SaxModbusTcpConnection::currentPhaseAChanged, thing, [thing](qint16 currentPhaseA){
        //     qCDebug(dcSax()) << "Smartmeter currentPhaseA changed" << currentPhaseA << "A";
        //     thing->setStateValue(saxMeterCurrentPhaseAStateTypeId, currentPhaseA);
        // });

        // /*smartmeter current phase B*/
        // connect(connection, &SaxModbusTcpConnection::currentPhaseBChanged, thing, [thing](qint16 currentPhaseB){
        //     qCDebug(dcSax()) << "Smartmeter currentPhaseB changed" << currentPhaseB << "A";
        //     thing->setStateValue(saxMeterCurrentPhaseBStateTypeId, currentPhaseB);
        // });

        // /*smartmeter current phase C*/
        // connect(connection, &SaxModbusTcpConnection::currentPhaseCChanged, thing, [thing](qint16 currentPhaseC){
        //     qCDebug(dcSax()) << "Smartmeter currentPhaseC changed" << currentPhaseC << "A";
        //     thing->setStateValue(saxMeterCurrentPhaseCStateTypeId, currentPhaseC);
        // });

        // /*smartmeter power phase A*/
        // connect(connection, &SaxModbusTcpConnection::powerPhaseAChanged, thing, [connection, thing](quint16 powerPhaseA){
        //     qint16 powerfactor = connection->powerFactorSmartmeter();
        //     double powerConverted = powerPhaseA * qPow(10, powerfactor);

        //     qCDebug(dcSax()) << "Smartmeter currentPowerowerPhaseA changed" << powerConverted << "W";
        //     thing->setStateValue(saxMeterCurrentPowerPhaseAStateTypeId, powerConverted);
        // });

        // /*smartmeter power phase B*/
        // connect(connection, &SaxModbusTcpConnection::powerPhaseBChanged, thing, [connection, thing](quint16 powerPhaseB){
        //     qint16 powerfactor = connection->powerFactorSmartmeter();
        //     double powerConverted = powerPhaseB * qPow(10, powerfactor);

        //     qCDebug(dcSax()) << "Smartmeter currentPowerowerPhaseB changed" << powerConverted << "W";
        //     thing->setStateValue(saxMeterCurrentPowerPhaseBStateTypeId, powerConverted);
        // });

        // /*smartmeter power phase C*/
        // connect(connection, &SaxModbusTcpConnection::powerPhaseCChanged, thing, [connection, thing](quint16 powerPhaseC){
        //     qint16 powerfactor = connection->powerFactorSmartmeter();
        //     double powerConverted = powerPhaseC * qPow(10, powerfactor);

        //     qCDebug(dcSax()) << "Smartmeter currentPowerPhaseC changed" << powerConverted << "W";
        //     thing->setStateValue(saxMeterCurrentPowerPhaseCStateTypeId, powerConverted);
        // });

        // /*smartmeter voltage phase A*/
        // connect(connection, &SaxModbusTcpConnection::voltagePhaseAChanged, thing, [thing](qint16 voltagePhaseA){
        //     qCDebug(dcSax()) << "Smartmeter voltagePhaseA changed" << voltagePhaseA << "V";
        //     thing->setStateValue(saxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
        // });

        // /*smartmeter voltage phase B*/
        // connect(connection, &SaxModbusTcpConnection::voltagePhaseBChanged, thing, [thing](qint16 voltagePhaseB){
        //     qCDebug(dcSax()) << "Smartmeter voltagePhaseB changed" << voltagePhaseB << "V";
        //     thing->setStateValue(saxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
        // });

        // /*smartmeter voltage phase C*/
        // connect(connection, &SaxModbusTcpConnection::voltagePhaseCChanged, thing, [thing](qint16 voltagePhaseC){
        //     qCDebug(dcSax()) << "Smartmeter voltagePhaseC changed" << voltagePhaseC << "V";
        //     thing->setStateValue(saxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
        // });

        /*battery SoC*/
        connect(connection, &SaxModbusTcpConnection::socBatteryChanged, thing, [thing](quint16 soc){
            qCDebug(dcSax()) << "Battery SoC changed" << soc << "%";
            if(soc < 20){
                thing->setStateValue(saxStorageBatteryCriticalStateTypeId, true);
            } else {
                thing->setStateValue(saxStorageBatteryCriticalStateTypeId, false);
            }
            thing->setStateValue(saxStorageBatteryLevelStateTypeId, soc);
        });

        /*battery capacity*/
        connect(connection, &SaxModbusTcpConnection::capacityChanged, thing, [thing](float capacity){
            qCDebug(dcSax()) << "Battery capacity changed" << capacity/1000 << "kWh";
            thing->setStateValue(saxStorageCapacityStateTypeId, capacity/1000);
        });

        /*battery cycles*/
        connect(connection, &SaxModbusTcpConnection::cyclesBatteryChanged, thing, [thing](quint16 cyclesBattery){
            qCDebug(dcSax()) << "Battery cyclesBattery changed" << cyclesBattery;
            thing->setStateValue(saxStorageCyclesBatteryStateTypeId, cyclesBattery);
        });

        /*battery temperature*/
        connect(connection, &SaxModbusTcpConnection::tempBatteryChanged, thing, [thing](quint16 tempBattery){
            qCDebug(dcSax()) << "Battery tempBattery changed" << tempBattery << "°C";
            thing->setStateValue(saxStorageTempBatteryStateTypeId, tempBattery);
        });

        connection->connectDevice();

        return;
    }

    if (thing->thingClassId() == saxMeterThingClassId) {
        qCDebug(dcSax()) << "SetUp Sax Meter";
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(saxMeterConnectedStateTypeId, parentThing->stateValue(saxStorageConnectedStateTypeId).toBool());
        }
        return;
    }

}

void IntegrationPluginSax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == saxStorageThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcSax()) << "Starting plugin timer...";
            // set refreshTime
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(REFRESH_TIME_MB_VALUES);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SaxModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->update() == false) {
                        qCWarning(dcSax()) << "Update failed, could not connect via Modbus to Sax storage";
                    }
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId).isEmpty()) {
            qCDebug(dcSax()) << "(Post) Set up sax meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(saxMeterThingClassId, "Sax Power Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginSax::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
