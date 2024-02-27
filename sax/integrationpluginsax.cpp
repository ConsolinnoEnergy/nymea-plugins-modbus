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

                ThingDescriptor descriptor(info->thingClassId(), thingClass.displayName() + " " + QString::number(result.capacity) + " kWh", "IP: " + result.networkDeviceInfo.address().toString());
                ParamList params{
                    {saxStorageThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress()},
                    {saxStorageThingModbusIdParamTypeId, result.modbusId},
                    {saxStorageThingPortParamTypeId, result.port}
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
            qCDebug(dcSax()) << "Reconfiguring existing thing" << thing->name();
            SaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();

        } else {
            qCDebug(dcSax()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));


        // Test for null, because registering a monitor with null will cause a segfault. Mac is loaded from config, you can't be sure config contains a valid mac.
        MacAddress macAddress = MacAddress(thing->paramValue(saxStorageThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSax()) << "Failed to set up Sax Battery because the MAC address is not valid:" << thing->paramValue(saxStorageThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcSax()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        qCDebug(dcSax()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(saxStorageThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info](bool reachable){
                qCDebug(dcSax()) << "Monitor reachable changed!" << reachable;
                if (reachable && !m_setupTcpConnectionRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_setupTcpConnectionRunning = true;
                    setupTcpConnection(info);
                }
            });
        }

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
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SaxModbusTcpConnection *connection, m_tcpConnections) {
                    qCDebug(dcSax()) << "Updating connection" << connection->hostAddress();
                    connection->update();
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
        SaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSax::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcSax()) << "setting up TCP connection";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    SaxModbusTcpConnection *connection = new SaxModbusTcpConnection(monitor->networkDeviceInfo().address(), thing->paramValue(saxStorageThingPortParamTypeId).toUInt(), thing->paramValue(saxStorageThingModbusIdParamTypeId).toUInt(), thing);
    m_tcpConnections.insert(thing, connection);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcSax()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        if (m_tcpConnections.contains(thing)) {
            m_tcpConnections.remove(thing);
        }
        connection->disconnectDevice();
        connection->deleteLater();
    });


    // Use a monitor to detect IP address changes. However, the monitor is not reliable and regularly reports 'not reachable' when the modbus connection
    // is still working. Because of that, check if the modbus connection is working before setting it to disconnect.
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [this, thing, monitor](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcSax()) << "Network device is now reachable for" << thing << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcSax()) << "Network device not reachable any more" << thing;
        }

        if (!thing->setupComplete())
            return;

        if (m_tcpConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(saxStorageConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (connection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    m_tcpConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_tcpConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    m_tcpConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &SaxModbusTcpConnection::reachableChanged, thing, [this, connection, thing, monitor](bool reachable){
        qCDebug(dcSax()) << "Reachable state changed" << reachable  << "for" << thing;
        thing->setStateValue(saxStorageConnectedStateTypeId, reachable);
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            meterThings.first()->setStateValue(saxMeterConnectedStateTypeId, reachable);
        }
        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(saxStorageCurrentPowerStateTypeId, 0);
            // Set all states 0. Sinnvoll, oder lieber nicht machen?
            // Pro: es fällt dem Nutzer eher auf, wenn es einen Fehler gibt. Höhere Warscheinlichkeit, das Fehler gemeldet werden
            // Contra: bei kurzen Ausfällen wird durch die 0 im State das Diagramm unschön. Wert drin lassen wäre da besser.

            // Wenn alle States 0, muss darauf geachtet werden, das sie danach auch wieder nicht 0 gesetzt werden. BatteryState z.B. würde
            // Probleme machen, weil der sich nicht so schnell ändert und deswegen kein update triggert.


            if (monitor->reachable()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            }
        }
    });

    // This connect is tied to "info", so it will be deleted when the setup has finished (= info is deleted).
    connect(connection, &SaxModbusTcpConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            qCDebug(dcSax()) << "Sax battery successfully initialized.";
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the battery."));
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

            qCDebug(dcSax()) << "Smartmeter frequency changed" << frequencyConverted << "Hz" << "( Factor"<< frequencyfactor << ")";
            meterThings.first()->setStateValue(saxMeterFrequencyStateTypeId, frequencyConverted);
        }
    });

    /*battery state of health*/
    connect(connection, &SaxModbusTcpConnection::stateOfHealthChanged, thing, [thing](quint16 stateOfHealth){
        qCDebug(dcSax()) << "Battery stateOfHealth changed" << stateOfHealth << "%";
        thing->setStateValue(saxStorageStateOfHealthStateTypeId, stateOfHealth);
    });

    /*battery state*/
    connect(connection, &SaxModbusTcpConnection::stateBatteryChanged, thing, [thing](SaxModbusTcpConnection::StateBattery stateBattery){
        qCDebug(dcSax()) << "Battery stateBattery changed" << stateBattery;
        switch (stateBattery) {
            case SaxModbusTcpConnection::StateBatteryOff:
                thing->setStateValue(saxStorageStateBatteryStateTypeId, "Off");
                break;
            case SaxModbusTcpConnection::StateBatteryOn:
                thing->setStateValue(saxStorageStateBatteryStateTypeId, "On");
                break;
            case SaxModbusTcpConnection::StateBatteryVerbund:
                thing->setStateValue(saxStorageStateBatteryStateTypeId, "Verbund");
                break;
            case SaxModbusTcpConnection::StateBatteryStandby:
                thing->setStateValue(saxStorageStateBatteryStateTypeId, "Standby");
                break;
            default:
                thing->setStateValue(saxStorageStateBatteryStateTypeId, "Off");
                break;
        }
    });


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
        qCDebug(dcSax()) << "Battery capacity changed" << capacity << "kWh";
        thing->setStateValue(saxStorageCapacityStateTypeId, capacity);
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


    /*Smartmeter connects*/
    /*smartmeter total energy produced*/
    connect(connection, &SaxModbusTcpConnection::totalEnergyProducedChanged, thing, [this, connection, thing](quint16 totalEnergyProduced){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qint16 energyfactor = connection->energyFactor();
            double energyConverted = totalEnergyProduced * qPow(10, energyfactor);

            qCDebug(dcSax()) << "Smartmeter totalEnergyProduced changed" << energyConverted;
            meterThings.first()->setStateValue(saxMeterTotalEnergyProducedStateTypeId, energyConverted);
        }
    });

    /*smartmeter total energy consumed*/
    connect(connection, &SaxModbusTcpConnection::totalEnergyConsumedChanged, thing, [this, connection, thing](quint16 totalEnergyConsumed){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qint16 energyfactor = connection->energyFactor();
            double energyConverted = totalEnergyConsumed * qPow(10, energyfactor);

            qCDebug(dcSax()) << "Smartmeter totalEnergyConsumed changed" << energyConverted;
            meterThings.first()->setStateValue(saxMeterTotalEnergyConsumedStateTypeId, energyConverted);
        }
    });



    /*smartmeter current phase A*/
    connect(connection, &SaxModbusTcpConnection::currentPhaseAChanged, thing, [this, thing](float currentPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter currentPhaseA changed" << currentPhase << "A";
            meterThings.first()->setStateValue(saxMeterCurrentPhaseAStateTypeId, currentPhase);
        }
    });

    /*smartmeter current phase B*/
    connect(connection, &SaxModbusTcpConnection::currentPhaseBChanged, thing, [this, thing](float currentPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter currentPhaseB changed" << currentPhase << "A";
            meterThings.first()->setStateValue(saxMeterCurrentPhaseBStateTypeId, currentPhase);
        }
    });

    /*smartmeter current phase C*/
    connect(connection, &SaxModbusTcpConnection::currentPhaseCChanged, thing, [this, thing](float currentPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter currentPhaseC changed" << currentPhase << "A";
            meterThings.first()->setStateValue(saxMeterCurrentPhaseCStateTypeId, currentPhase);
        }
    });

    /*smartmeter power phase A*/
    connect(connection, &SaxModbusTcpConnection::powerPhaseAChanged, thing, [this, connection, thing](quint16 powerPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qint16 powerfactor = connection->powerFactorSmartmeter();
            double powerConverted = powerPhase * qPow(10, powerfactor);

            qCDebug(dcSax()) << "Smartmeter currentPowerowerPhaseA changed" << powerConverted << "W";
            meterThings.first()->setStateValue(saxMeterCurrentPowerPhaseAStateTypeId, powerConverted);
        }
    });

    /*smartmeter power phase B*/
    connect(connection, &SaxModbusTcpConnection::powerPhaseBChanged, thing, [this, connection, thing](quint16 powerPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qint16 powerfactor = connection->powerFactorSmartmeter();
            double powerConverted = powerPhase * qPow(10, powerfactor);

            qCDebug(dcSax()) << "Smartmeter currentPowerowerPhaseB changed" << powerConverted << "W";
            meterThings.first()->setStateValue(saxMeterCurrentPowerPhaseBStateTypeId, powerConverted);
        }
    });

    /*smartmeter power phase C*/
    connect(connection, &SaxModbusTcpConnection::powerPhaseCChanged, thing, [this, connection, thing](quint16 powerPhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qint16 powerfactor = connection->powerFactorSmartmeter();
            double powerConverted = powerPhase * qPow(10, powerfactor);

            qCDebug(dcSax()) << "Smartmeter currentPowerowerPhaseC changed" << powerConverted << "W";
            meterThings.first()->setStateValue(saxMeterCurrentPowerPhaseCStateTypeId, powerConverted);
        }
    });

    /*smartmeter voltage phase A*/
    connect(connection, &SaxModbusTcpConnection::voltagePhaseAChanged, thing, [this, thing](float voltagePhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter voltagePhaseA changed" << voltagePhase << "V";
            meterThings.first()->setStateValue(saxMeterVoltagePhaseAStateTypeId, voltagePhase);
        }
    });

    /*smartmeter voltage phase B*/
    connect(connection, &SaxModbusTcpConnection::voltagePhaseBChanged, thing, [this, thing](float voltagePhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter voltagePhaseB changed" << voltagePhase << "V";
            meterThings.first()->setStateValue(saxMeterVoltagePhaseBStateTypeId, voltagePhase);
        }
    });

    /*smartmeter voltage phase C*/
    connect(connection, &SaxModbusTcpConnection::voltagePhaseCChanged, thing, [this, thing](float voltagePhase){
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(saxMeterThingClassId);
        if (!meterThings.isEmpty()) {
            qCDebug(dcSax()) << "Smartmeter voltagePhaseC changed" << voltagePhase << "V";
            meterThings.first()->setStateValue(saxMeterVoltagePhaseCStateTypeId, voltagePhase);
        }
    });


    connection->connectDevice();
}
