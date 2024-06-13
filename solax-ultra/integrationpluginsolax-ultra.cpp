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

#include "integrationpluginsolax-ultra.h"
#include "plugininfo.h"
#include "discoverytcp.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QEventLoop>

IntegrationPluginSolax::IntegrationPluginSolax()
{

}

void IntegrationPluginSolax::init()
{
}

void IntegrationPluginSolax::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxX3UltraThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxUltra()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
            foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxX3UltraThingClassId, result.manufacturerName + " Inverter " + result.productName, QString("rated power: %1").arg(result.powerRating) + "W - " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxUltra()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxX3UltraThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolaxUltra()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxX3UltraThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxX3UltraThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxX3UltraThingPortParamTypeId, result.port);
                params << Param(solaxX3UltraThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();

    }
}

void IntegrationPluginSolax::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolaxUltra()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == solaxX3UltraThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSolaxUltra()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            SolaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolaxUltra()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolaxUltra()) << "Failed to set up Solax inverter because the MAC address is not valid:" << thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own.
        // In this call, nymea is checking a list for known mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip address and is set to not reachable
        m_monitors.insert(thing, monitor);

        // If the ip address was not found in the cache, wait for the for the network discovery cache to be updated
        qCDebug(dcSolaxUltra()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(solaxX3UltraThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) 
        {
            setupTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(), &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                if (!m_setupTcpConnectionRunning)
                {
                    m_setupTcpConnectionRunning = true;
                    setupTcpConnection(info);
                }
            });
        }
    }

    if (thing->thingClassId() == solaxMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3UltraConnectedStateTypeId).toBool());
            }
        }
        return;
    }

    if (thing->thingClassId() == solaxBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (m_batterystates.value(parentThing).bmsCommStatus && m_batterystates.value(parentThing).modbusReachable) {
                thing->setStateValue(solaxBatteryConnectedStateTypeId, true);
            } else {
                thing->setStateValue(solaxBatteryConnectedStateTypeId, false);
                thing->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
            }

            // Set capacity from parent parameter.
            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3UltraThingBatteryCapacityParamTypeId).toUInt());
            }

            BatteryStates batteryStates{};
            m_batterystates.insert(thing, batteryStates);
        }
        return;
    }

    if (thing->thingClassId() == solaxBattery2ThingClassId) {
        // Nothing to to here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (m_batterystates.value(parentThing).bmsCommStatus && m_batterystates.value(parentThing).modbusReachable) {
                thing->setStateValue(solaxBattery2ConnectedStateTypeId, true);
            } else {
                thing->setStateValue(solaxBattery2ConnectedStateTypeId, false);
                thing->setStateValue(solaxBattery2CurrentPowerStateTypeId, 0);
            }

            if (parentThing->thingClassId() == solaxX3UltraThingClassId) {
                thing->setStateValue(solaxBattery2CapacityStateTypeId, parentThing->paramValue(solaxX3UltraThingBattery2CapacityParamTypeId).toUInt());
            }

            BatteryStates batteryStates{};
            m_batterystates.insert(thing, batteryStates);
        }
        return;
    }
}

void IntegrationPluginSolax::setupTcpConnection(ThingSetupInfo *info)
{
        qCDebug(dcSolaxUltra()) << "Setup TCP connection.";
        Thing *thing = info->thing();
        NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
        uint port = thing->paramValue(solaxX3UltraThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxX3UltraThingModbusIdParamTypeId).toUInt();
        SolaxModbusTcpConnection *connection = new SolaxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        m_tcpConnections.insert(thing, connection);
        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Is this needed? How can setup be aborted at this point?

            // Clean up in case the setup gets aborted.
            if (m_monitors.contains(thing)) {
                qCDebug(dcSolaxUltra()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // Reconnect on monitor reachable changed.
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSolaxUltra()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxX3UltraConnectedStateTypeId).toBool()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            } else if (!reachable) {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again.
                connection->disconnectDevice();
            }
        });

        connect(connection, &SolaxModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable){
            qCDebug(dcSolaxUltra()) << "Reachable state changed" << reachable;
            if (reachable) {
                // Connected true will be set after successfull init.
                connection->initialize();
            } else {
                thing->setStateValue(solaxX3UltraConnectedStateTypeId, false);
                thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, 0);
                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }

                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue(solaxX3UltraConnectedStateTypeId, success);

            // Set connected state for meter
            m_meterstates.find(thing)->modbusReachable = success;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                if (success && m_meterstates.value(thing).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            m_batterystates.find(thing)->modbusReachable = success;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (success && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                }
            }

            if (success) {
                qCDebug(dcSolaxUltra()) << "Solax inverter initialized.";
                thing->setStateValue(solaxX3UltraFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxX3UltraRatedPowerStateTypeId, connection->inverterType());
            } else {
                qCDebug(dcSolaxUltra()) << "Solax inverter initialization failed.";
                // Try once to reconnect the device
                connection->reconnectDevice();
            } 
        });


        // Handle property changed signals for inverter
        connect(connection, &SolaxModbusTcpConnection::inverterPowerChanged, thing, [thing, this](double inverterPower){
            // TODO: use EoutPowerTotal
            qCDebug(dcSolaxUltra()) << "Inverter power changed" << inverterPower << "W";
            // https://consolinno.atlassian.net/wiki/spaces/~62f39a8532850ea2a3268713/pages/462848121/Bugfixing+Solax+EX3
           
            double batteryPower = 0;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                batteryPower = batteryThings.first()->stateValue(solaxBatteryCurrentPowerStateTypeId).toDouble();
            }
           
            if (batteryPower < 0)
            {
                // Battery is discharging
                batteryPower = 0;
            }
            qCDebug(dcSolaxUltra()) << "Subtract from InverterPower";
            // thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, -inverterPower-batteryPower);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterVoltageChanged, thing, [thing](double inverterVoltage){
            qCDebug(dcSolaxUltra()) << "Inverter voltage changed" << inverterVoltage << "V";
            thing->setStateValue(solaxX3UltraInverterVoltageStateTypeId, inverterVoltage);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterCurrentChanged, thing, [thing](double inverterCurrent){
            qCDebug(dcSolaxUltra()) << "Inverter current changed" << inverterCurrent << "A";
            thing->setStateValue(solaxX3UltraInverterCurrentStateTypeId, inverterCurrent);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc1Changed, thing, [thing](double powerDc1){
            qCDebug(dcSolaxUltra()) << "Inverter DC1 power changed" << powerDc1 << "W";
            thing->setStateValue(solaxX3UltraPv1PowerStateTypeId, powerDc1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage1Changed, thing, [thing](double pvVoltage1){
            qCDebug(dcSolaxUltra()) << "Inverter PV1 voltage changed" << pvVoltage1 << "V";
            thing->setStateValue(solaxX3UltraPv1VoltageStateTypeId, pvVoltage1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent1Changed, thing, [thing](double pvCurrent1){
            qCDebug(dcSolaxUltra()) << "Inverter PV1 current changed" << pvCurrent1 << "A";
            thing->setStateValue(solaxX3UltraPv1CurrentStateTypeId, pvCurrent1);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc2Changed, thing, [thing](double powerDc2){
            qCDebug(dcSolaxUltra()) << "Inverter DC2 power changed" << powerDc2 << "W";
            thing->setStateValue(solaxX3UltraPv2PowerStateTypeId, powerDc2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage2Changed, thing, [thing](double pvVoltage2){
            qCDebug(dcSolaxUltra()) << "Inverter PV2 voltage changed" << pvVoltage2 << "V";
            thing->setStateValue(solaxX3UltraPv2VoltageStateTypeId, pvVoltage2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent2Changed, thing, [thing](double pvCurrent2){
            qCDebug(dcSolaxUltra()) << "Inverter PV2 current changed" << pvCurrent2 << "A";
            thing->setStateValue(solaxX3UltraPv2CurrentStateTypeId, pvCurrent2);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc3Changed, thing, [thing](double powerDc3){
            qCDebug(dcSolaxUltra()) << "Inverter DC3 power changed" << powerDc3 << "W";
            thing->setStateValue(solaxX3UltraPv3PowerStateTypeId, powerDc3);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage3Changed, thing, [thing](double pvVoltage3){
            qCDebug(dcSolaxUltra()) << "Inverter PV3 voltage changed" << pvVoltage3 << "V";
            thing->setStateValue(solaxX3UltraPv3VoltageStateTypeId, pvVoltage3);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent3Changed, thing, [thing](double pvCurrent3){
            qCDebug(dcSolaxUltra()) << "Inverter PV3 current changed" << pvCurrent3 << "A";
            thing->setStateValue(solaxX3UltraPv3CurrentStateTypeId, pvCurrent3);
        });

        connect(connection, &SolaxModbusTcpConnection::runModeChanged, thing, [this, thing](SolaxModbusTcpConnection::RunMode runMode){
            qCDebug(dcSolaxUltra()) << "Inverter run mode recieved" << runMode;
            quint16 runModeAsInt = runMode;
            setRunMode(thing, runModeAsInt);
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureChanged, thing, [thing](quint16 temperature){
            qCDebug(dcSolaxUltra()) << "Inverter temperature changed" << temperature << "°C";
            thing->setStateValue(solaxX3UltraTemperatureStateTypeId, temperature);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTotalChanged, thing, [thing](double solarEnergyTotal){
            qCDebug(dcSolaxUltra()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";
            thing->setStateValue(solaxX3UltraTotalEnergyProducedStateTypeId, solarEnergyTotal);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolaxUltra()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3UltraEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxModbusTcpConnection::activePowerLimitChanged, thing, [thing](quint16 activePowerLimit){
            qCDebug(dcSolaxUltra()) << "Inverter active power limit changed" << activePowerLimit << "%";
            thing->setStateValue(solaxX3UltraActivePowerLimitStateTypeId, activePowerLimit);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolaxUltra()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });


        // Meter
        connect(connection, &SolaxModbusTcpConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            m_meterstates.find(thing)->meterCommStatus = commStatusBool;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(thing).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                // Sign should be correct, but check to make sure.
                meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -double(feedinPower));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentRChanged, thing, [this, thing](double currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase A current (gridCurrentR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentSChanged, thing, [this, thing](double currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase B current (gridCurrentS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentTChanged, thing, [this, thing](double currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase C current (gridCurrentT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase A power (gridPowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseAStateTypeId, double(currentPowerPhaseA));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase B power (gridPowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, double(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, double(currentPowerPhaseC));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageRChanged, thing, [this, thing](double voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase A voltage (gridVoltageR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageSChanged, thing, [this, thing](double voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase B voltage (gridVoltageS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageTChanged, thing, [this, thing](double voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase C voltage (gridVoltageT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyRChanged, thing, [this, thing](double gridFrequencyR){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase A frequency (gridFrequencyR) changed" << gridFrequencyR << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseAStateTypeId, gridFrequencyR);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencySChanged, thing, [this, thing](double gridFrequencyS){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase B frequency (gridFrequencyS) changed" << gridFrequencyS << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseBStateTypeId, gridFrequencyS);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyTChanged, thing, [this, thing](double gridFrequencyT){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter phase C frequency (gridFrequencyT) changed" << gridFrequencyT << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseCStateTypeId, gridFrequencyT);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });


        // Battery        
        connect(connection, &SolaxModbusTcpConnection::bmsConnectStateChanged, thing, [this, thing](quint16 bmsConnect){
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcSolaxUltra()) << "Battery connect state (bmsConnectState) changed" << bmsConnect;
            bool bmsCommStatusBool = (bmsConnect != 0);
            m_batterystates.find(thing)->bmsCommStatus = bmsCommStatusBool;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcSolaxUltra()) << "Set up Solax battery for" << thing;
                ThingDescriptor descriptor(solaxBatteryThingClassId, "Solax battery", QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batPowerCharge1Changed, thing, [this, thing](qint16 powerBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery power (batpowerCharge1) changed" << powerBat1 << "W";

                // ToDo: Check if sign for charge / dischage is correct.
                batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, double(powerBat1));
                if (powerBat1 < 0) {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batteryCapacityChanged, thing, [this, thing](quint16 socBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";
                batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolaxUltra()) << "Battery warning bits LSB recieved" << batteryWarningBitsLsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            Thing* batteryThing = batteryThings.first();
            m_batterystates.find(batteryThing)->bmsWarningLsb = batteryWarningBitsLsb;
            setBmsWarningMessage(batteryThing);

        });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningMsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolaxUltra()) << "Battery warning bits MSB recieved" << batteryWarningBitsMsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            Thing* batteryThing = batteryThings.first();
            m_batterystates.find(batteryThing)->bmsWarningMsb = batteryWarningBitsMsb;
            setBmsWarningMessage(batteryThing);
        });

        connect(connection, &SolaxModbusTcpConnection::batVoltageCharge1Changed, thing, [this, thing](double batVoltageCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery voltage changed" << batVoltageCharge1 << "V";
                batteryThings.first()->setStateValue(solaxBatteryBatteryVoltageStateTypeId, batVoltageCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batCurrentCharge1Changed, thing, [this, thing](double batCurrentCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery current changed" << batCurrentCharge1 << "A";
                batteryThings.first()->setStateValue(solaxBatteryBatteryCurrentStateTypeId, batCurrentCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureBatChanged, thing, [this, thing](quint16 temperatureBat){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery temperature changed" << temperatureBat << "°C";
                batteryThings.first()->setStateValue(solaxBatteryTemperatureStateTypeId, temperatureBat);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batVoltageCharge2Changed, thing, [this, thing](double batVoltCharge2) {
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Batter 2 voltage changed" << batVoltCharge2 << "W";
                batteryThings.first()->setStateValue(solaxBattery2BatteryVoltageStateTypeId, batVoltCharge2);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batCurrentCharge2Changed, thing, [this, thing](double batCurrentCharge2){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery 2 current changed" << batCurrentCharge2 << "A";
                batteryThings.first()->setStateValue(solaxBattery2BatteryCurrentStateTypeId, batCurrentCharge2);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureBat2Changed, thing, [this, thing](qint16 bat2Temperature) {
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Batter 2 temperature changed" << bat2Temperature << "°C";
                batteryThings.first()->setStateValue(solaxBattery2TemperatureStateTypeId, bat2Temperature);
            }
        });

        // TODO 3 -> 2
        connect(connection, &SolaxModbusTcpConnection::batPowerCharge3Changed, thing, [this, thing](qint16 powerBat2){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery power (batpowerCharge2) changed" << powerBat2 << "W";

                // ToDo: Check if sign for charge / dischage is correct.
                batteryThings.first()->setStateValue(solaxBattery2CurrentPowerStateTypeId, double(powerBat2));
                if (powerBat2 < 0) {
                    batteryThings.first()->setStateValue(solaxBattery2ChargingStateStateTypeId, "discharging");
                } else if (powerBat2 > 0) {
                    batteryThings.first()->setStateValue(solaxBattery2ChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(solaxBattery2ChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batteryCapacity2Changed, thing, [this, thing](quint16 socBat2){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolaxUltra()) << "Battery 2 state of charge (batteryCapacity) changed" << socBat2 << "%";
                batteryThings.first()->setStateValue(solaxBattery2BatteryLevelStateTypeId, socBat2);
                batteryThings.first()->setStateValue(solaxBattery2BatteryCriticalStateTypeId, socBat2 < 10);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::bms2FaultLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolaxUltra()) << "Battery 2 warning bits LSB recieved" << batteryWarningBitsLsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            Thing* batteryThing = batteryThings.first();
            m_batterystates.find(batteryThing)->bmsWarningLsb = batteryWarningBitsLsb;
            setBmsWarningMessage(batteryThing);
        });

        connect(connection, &SolaxModbusTcpConnection::bms2FaulMLsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolaxUltra()) << "Battery 2 warning bits MSB recieved" << batteryWarningBitsMsb;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBattery2ThingClassId);
            Thing* batteryThing = batteryThings.first();
            m_batterystates.find(batteryThing)->bmsWarningMsb = batteryWarningBitsMsb;
            setBmsWarningMessage(batteryThing);
        });

        connect(connection, &SolaxModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
            // TODO: handle currentpower in line 294
            qCDebug(dcSolaxUltra()) << "Calculate total inverter power";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            quint16 powerDc3 = connection->powerDc3();
            thing->setStateValue(solaxX3UltraCurrentPowerStateTypeId, -(powerDc1+powerDc2+powerDc3));
        });


        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

        return;
}

void IntegrationPluginSolax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == solaxX3UltraThingClassId) {

        bool connectionWasCreated = m_tcpConnections.contains(thing);
        if (!connectionWasCreated) {
            qCDebug(dcSolaxUltra()) << "Aborting post setup, because setup did not complete.";
            return;
        }

        if (!m_pluginTimer) {
            qCDebug(dcSolaxUltra()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SolaxModbusTcpConnection *connection, m_tcpConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId).isEmpty()) {
            qCDebug(dcSolaxUltra()) << "Set up solax meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(solaxMeterThingClassId, "Solax Power Meter", QString(), thing->id()));
        }
    }
}

// Code for setting power limit. Disabled for now.
/*
void IntegrationPluginSolax::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == solaxX3UltraThingClassId) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxUltra()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSolaxUltra()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxX3UltraActivePowerLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3UltraActivePowerLimitActionActivePowerLimitParamTypeId).toUInt();
            qCDebug(dcSolaxUltra()) << "Trying to set active power limit to" << powerLimit;
            QModbusReply *reply = connection->setActivePowerLimit(powerLimit);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, thing, reply, powerLimit](){
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSolaxUltra()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCDebug(dcSolaxUltra()) << "Active power limit set to" << powerLimit;
                    thing->setStateValue(solaxX3UltraActivePowerLimitStateTypeId, powerLimit);
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolaxUltra()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->modbusRtuMaster()->connected()) {
            qCWarning(dcSolaxUltra()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);2
            return;
        }

        if (action.actionTypeId() == solaxX3InverterRTUActivePowerLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3InverterRTUActivePowerLimitActionActivePowerLimitParamTypeId).toUInt();
            qCDebug(dcSolaxUltra()) << "Trying to set active power limit to" << powerLimit;
            ModbusRtuReply *reply = connection->setActivePowerLimit(powerLimit);
            connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
            connect(reply, &ModbusRtuReply::finished, info, [info, thing, reply, powerLimit](){
                if (reply->error() != ModbusRtuReply::NoError) {
                    qCWarning(dcSolaxUltra()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCDebug(dcSolaxUltra()) << "Active power limit set to" << powerLimit;
                    thing->setStateValue(solaxX3InverterRTUActivePowerLimitStateTypeId, powerLimit);
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    }
}
*/

void IntegrationPluginSolax::thingRemoved(Thing *thing)
{
    if (m_tcpConnections.contains(thing)) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_meterstates.contains(thing)) {
        m_meterstates.remove(thing);
    }

    if (m_batterystates.contains(thing)) {
        m_batterystates.remove(thing);
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSolax::setRunMode(Thing *thing, quint16 runModeAsInt)
{
    QString runModeAsString{"Unknown"};
    switch (runModeAsInt) {
        case 0:
            runModeAsString = "Waiting";
            break;
        case 1:
            runModeAsString = "Checking";
            break;
        case 2:
            runModeAsString = "Normal";
            break;
        case 3:
            runModeAsString = "Fault";
            break;
        case 4:
            runModeAsString = "Permanent fault";
            break;
        case 5:
            runModeAsString = "Updating";
            break;
        case 6:
            runModeAsString = "EPS Check";
            break;
        case 7:
            runModeAsString = "EPS";
            break;
        case 8:
            runModeAsString = "Self Test";
            break;
        case 9:
            runModeAsString = "Idle";
            break;
    }
    if (thing->thingClassId() == solaxX3UltraThingClassId) {
        thing->setStateValue(solaxX3UltraInverterStatusStateTypeId, runModeAsString);
    }
}

void IntegrationPluginSolax::setErrorMessage(Thing *thing, quint32 errorBits)
{
    QString errorMessage{""};
    if (errorBits == 0) {
        errorMessage = "No error, everything ok.";
    } else {
        errorMessage.append("The following error bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (errorBits & (testBit << var)) {
                errorMessage.append(QStringLiteral("bit %1").arg(var));
                if (var == 0) {
                    errorMessage.append(" HardwareTrip, ");
                } else if (var == 1) {
                    errorMessage.append(" MainsLostFault, ");
                } else if (var == 2) {
                    errorMessage.append(" GridVoltFault, ");
                } else if (var == 3) {
                    errorMessage.append(" GridFreqFault, ");
                } else if (var == 4) {
                    errorMessage.append(" PvVoltFault, ");
                } else if (var == 5) {
                    errorMessage.append(" BusVoltFault, ");
                } else if (var == 6) {
                    errorMessage.append(" Bat Volt Fault, ");
                } else if (var == 7) {
                    errorMessage.append(" Ac10Mins_Voltage_Fault, ");
                } else if (var == 8) {
                    errorMessage.append(" Dci_OCP_Fault, ");
                } else if (var == 9) {
                    errorMessage.append(" Dcv_OCP_Fault, ");
                } else if (var == 10) {
                    errorMessage.append(" SW_OCP_Fault, ");
                } else if (var == 11) {
                    errorMessage.append(" RC_OCP_Fault, ");
                } else if (var == 12) {
                    errorMessage.append(" IsolationFault, ");
                } else if (var == 13) {
                    errorMessage.append(" TemperatureOverFault, ");
                } else if (var == 14) {
                    errorMessage.append(" BatConDir_Fault, ");
                } else if (var == 15) {
                    errorMessage.append(" SampleConsistenceFault, ");
                } else if (var == 16) {
                    errorMessage.append(" EpsOverLoad, ");
                } else if (var == 17) {
                    errorMessage.append(" EPS_OCP_Fault, ");
                } else if (var == 18) {
                    errorMessage.append(" InputConfigFault, ");
                } else if (var == 19) {
                    errorMessage.append(" FirmwareVerFault, ");
                } else if (var == 20) {
                    errorMessage.append(" EPSBatPowerLow, ");
                } else if (var == 21) {
                    errorMessage.append(" PhaseAngleFault, ");
                } else if (var == 22) {
                    errorMessage.append(" PLL_OverTime, ");
                } else if (var == 23) {
                    errorMessage.append(" ParallelFault, ");
                } else if (var == 24) {
                    errorMessage.append(" Inter_Com_Fault, ");
                } else if (var == 25) {
                    errorMessage.append(" Fan Fault, ");
                } else if (var == 26) {
                    errorMessage.append(" HCT_AC_DeviceFault, ");
                } else if (var == 27) {
                    errorMessage.append(" EepromFault, ");
                } else if (var == 28) {
                    errorMessage.append(" ResidualCurrent_DeviceFault, ");
                } else if (var == 29) {
                    errorMessage.append(" EpsRelayFault, ");
                } else if (var == 30) {
                    errorMessage.append(" GridRelayFault, ");
                } else if (var == 31) {
                    errorMessage.append(" BatRelayFault, ");
                } else {
                    errorMessage.append(", ");
                }
            }
        }
        int stringLength = errorMessage.length();
        if (stringLength > 1) { // stringLength should be > 1, but just in case.
            errorMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
        }
    }
    qCDebug(dcSolaxUltra()) << errorMessage;
    if (thing->thingClassId() == solaxX3UltraThingClassId) {
        thing->setStateValue(solaxX3UltraErrorMessageStateTypeId, errorMessage);
    }
}

void IntegrationPluginSolax::setBmsWarningMessage(Thing *thing)
{
    QString warningMessage{""};
    quint16 warningBitsLsb = m_batterystates.value(thing).bmsWarningLsb;
    quint16 warningBitsMsb = m_batterystates.value(thing).bmsWarningMsb;

    quint32 warningBits = warningBitsMsb;
    warningBits = (warningBits << 16) + warningBitsLsb;

    if (warningBits == 0) {
        warningMessage = "No warning, everything ok.";
    } else {
        warningMessage.append("The following warning bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (warningBits & (testBit << var)) {
                warningMessage.append(QStringLiteral("bit %1").arg(var));
                if (var == 0) {
                    warningMessage.append(" BMS_External_Err, ");
                } else if (var == 1) {
                    warningMessage.append(" BMS_Internal_Err, ");
                } else if (var == 2) {
                    warningMessage.append(" BMS_OverVoltage, ");
                } else if (var == 3) {
                    warningMessage.append(" BMS_LowerVoltage, ");
                } else if (var == 4) {
                    warningMessage.append(" BMS_ChargeOverCurrent, ");
                } else if (var == 5) {
                    warningMessage.append(" BMS_DishargeOverCurrent, ");
                } else if (var == 6) {
                    warningMessage.append(" BMS_TemHigh, ");
                } else if (var == 7) {
                    warningMessage.append(" BMS_TemLow, ");
                } else if (var == 8) {
                    warningMessage.append(" BMS_CellImblance, ");
                } else if (var == 9) {
                    warningMessage.append(" BMS_Hardware_Prot, ");
                } else if (var == 10) {
                    warningMessage.append(" BMS_Inlock_Fault, ");
                } else if (var == 11) {
                    warningMessage.append(" BMS_ISO_Fault, ");
                } else if (var == 12) {
                    warningMessage.append(" BMS_VolSen_Fault, ");
                } else if (var == 13) {
                    warningMessage.append(" BMS_TempSen_Fault, ");
                } else if (var == 14) {
                    warningMessage.append(" BMS_CurSen_Fault, ");
                } else if (var == 15) {
                    warningMessage.append(" BMS_Relay_Fault, ");
                } else if (var == 16) {
                    warningMessage.append(" BMS_Type_Unmatch, ");
                } else if (var == 17) {
                    warningMessage.append(" BMS_Ver_Unmatch, ");
                } else if (var == 18) {
                    warningMessage.append(" BMS_Manufacturer_Unmatch, ");
                } else if (var == 19) {
                    warningMessage.append(" BMS_SW&HW_Unmatch, ");
                } else if (var == 20) {
                    warningMessage.append(" BMS_M&S_Unmatch, ");
                } else if (var == 21) {
                    warningMessage.append(" BMS_CR_Unresponsive, ");
                } else if (var == 22) {
                    warningMessage.append(" BMS_Software_Protect, ");
                } else if (var == 23) {
                    warningMessage.append(" BMS_536_Fault, ");
                } else if (var == 24) {
                    warningMessage.append(" Selfchecking_Fault, ");
                } else if (var == 25) {
                    warningMessage.append(" BMS_Tempdiff_Fault, ");
                } else if (var == 26) {
                    warningMessage.append(" BMS_Break, ");
                } else if (var == 27) {
                    warningMessage.append(" BMS_Flash_Fault, ");
                } else {
                    warningMessage.append(", ");
                }
            }
        }
        int stringLength = warningMessage.length();
        if (stringLength > 1) { // stringLength should be > 1, but just in case.
            warningMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
        }
    }
    qCDebug(dcSolaxUltra()) << warningMessage;
    thing->setStateValue(solaxBatteryWarningMessageStateTypeId, warningMessage);
}
