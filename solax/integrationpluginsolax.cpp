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

#include "integrationpluginsolax.h"
#include "plugininfo.h"
#include "discoveryrtu.h"
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
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcSolax()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(solaxX3InverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcSolax()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(solaxX3InverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    // Support for multiple batteries.
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(solaxBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginSolax::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxX3InverterTCPThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolax()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
            foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {

                QString name = supportedThings().findById(solaxX3InverterTCPThingClassId).displayName();
                ThingDescriptor descriptor(solaxX3InverterTCPThingClassId, name, QString("rated power: %1").arg(result.powerRating) + "W - " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolax()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxX3InverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolax()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxX3InverterTCPThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxX3InverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxX3InverterTCPThingPortParamTypeId, result.port);
                params << Param(solaxX3InverterTCPThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();

    } else if (info->thingClassId() == solaxX3InverterRTUThingClassId) {
        qCDebug(dcSolax()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), info);
        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with appropriate settings found."));
                return;
            }

            qCInfo(dcSolax()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {
                QString name = supportedThings().findById(solaxX3InverterRTUThingClassId).displayName();
                ThingDescriptor descriptor(info->thingClassId(), name, QString("rated power: %1").arg(result.powerRating) + "W - " + QString("Modbus ID: %1").arg(result.modbusId));

                ParamList params{
                    {solaxX3InverterRTUThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {solaxX3InverterRTUThingModbusIdParamTypeId, result.modbusId}
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

void IntegrationPluginSolax::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolax()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSolax()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            SolaxModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolax()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(solaxX3InverterTCPThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolax()) << "Failed to set up Solax inverter because the MAC address is not valid:" << thing->paramValue(solaxX3InverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
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
        qCDebug(dcSolax()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(solaxX3InverterTCPThingMacAddressParamTypeId).toString();
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

    if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {

        uint address = thing->paramValue(solaxX3InverterRTUThingModbusIdParamTypeId).toUInt();
        if (address > 247 || address == 0) {
            qCWarning(dcSolax()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(solaxX3InverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcSolax()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcSolax()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        SolaxModbusRtuConnection *connection = new SolaxModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &SolaxModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(solaxX3InverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            m_meterstates.find(thing)->modbusReachable = reachable;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                if (reachable && m_meterstates.value(thing).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            m_batterystates.find(thing)->modbusReachable = reachable;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (reachable && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                }
            }

            if (reachable) {
                qCDebug(dcSolax()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcSolax()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });


        // Handle property changed signals for inverter
        connect(connection, &SolaxModbusRtuConnection::initializationFinished, thing, [=](bool success){
            if (success) {
                qCDebug(dcSolax()) << "Solax inverter initialized.";
                thing->setStateValue(solaxX3InverterRTUFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxX3InverterRTUNominalPowerStateTypeId, connection->inverterType());
                thing->setStateValue(solaxX3InverterRTUExportLimitStateTypeId, connection->inverterType());
            } else {
                qCDebug(dcSolax()) << "Solax inverter initialization failed.";
            }
        });

        connect(connection, &SolaxModbusRtuConnection::inverterVoltageChanged, thing, [thing](double inverterVoltage){
            qCDebug(dcSolax()) << "Inverter voltage changed" << inverterVoltage << "V";
            thing->setStateValue(solaxX3InverterRTUInverterVoltageStateTypeId, inverterVoltage);
        });

        connect(connection, &SolaxModbusRtuConnection::inverterCurrentChanged, thing, [thing](double inverterCurrent){
            qCDebug(dcSolax()) << "Inverter current changed" << inverterCurrent << "A";
            thing->setStateValue(solaxX3InverterRTUInverterCurrentStateTypeId, inverterCurrent);
        });

        connect(connection, &SolaxModbusRtuConnection::powerDc1Changed, thing, [thing](double powerDc1){
            qCDebug(dcSolax()) << "Inverter DC1 power changed" << powerDc1 << "W";
            thing->setStateValue(solaxX3InverterRTUPv1PowerStateTypeId, powerDc1);
        });

        connect(connection, &SolaxModbusRtuConnection::pvVoltage1Changed, thing, [thing](double pvVoltage1){
            qCDebug(dcSolax()) << "Inverter PV1 voltage changed" << pvVoltage1 << "V";
            thing->setStateValue(solaxX3InverterRTUPv1VoltageStateTypeId, pvVoltage1);
        });

        connect(connection, &SolaxModbusRtuConnection::pvCurrent1Changed, thing, [thing](double pvCurrent1){
            qCDebug(dcSolax()) << "Inverter PV1 current changed" << pvCurrent1 << "A";
            thing->setStateValue(solaxX3InverterRTUPv1CurrentStateTypeId, pvCurrent1);
        });

        connect(connection, &SolaxModbusRtuConnection::powerDc2Changed, thing, [thing](double powerDc2){
            qCDebug(dcSolax()) << "Inverter DC2 power changed" << powerDc2 << "W";
            thing->setStateValue(solaxX3InverterRTUPv2PowerStateTypeId, powerDc2);
        });

        connect(connection, &SolaxModbusRtuConnection::pvVoltage2Changed, thing, [thing](double pvVoltage2){
            qCDebug(dcSolax()) << "Inverter PV2 voltage changed" << pvVoltage2 << "V";
            thing->setStateValue(solaxX3InverterRTUPv2VoltageStateTypeId, pvVoltage2);
        });

        connect(connection, &SolaxModbusRtuConnection::pvCurrent2Changed, thing, [thing](double pvCurrent2){
            qCDebug(dcSolax()) << "Inverter PV2 current changed" << pvCurrent2 << "A";
            thing->setStateValue(solaxX3InverterRTUPv2CurrentStateTypeId, pvCurrent2);
        });

        connect(connection, &SolaxModbusRtuConnection::runModeChanged, thing, [this, thing](SolaxModbusRtuConnection::RunMode runMode){
            qCDebug(dcSolax()) << "Inverter run mode recieved" << runMode;
            quint16 runModeAsInt = runMode;
            setRunMode(thing, runModeAsInt);
        });

        connect(connection, &SolaxModbusRtuConnection::temperatureChanged, thing, [thing](quint16 temperature){
            qCDebug(dcSolax()) << "Inverter temperature changed" << temperature << "°C";
            thing->setStateValue(solaxX3InverterRTUTemperatureStateTypeId, temperature);
        });

        connect(connection, &SolaxModbusRtuConnection::solarEnergyTotalChanged, thing, [thing](double solarEnergyTotal){
            qCDebug(dcSolax()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";
            thing->setStateValue(solaxX3InverterRTUTotalEnergyProducedStateTypeId, solarEnergyTotal);
        });

        connect(connection, &SolaxModbusRtuConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolax()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3InverterRTUEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxModbusRtuConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolax()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });


        // Meter
        connect(connection, &SolaxModbusRtuConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            m_meterstates.find(thing)->meterCommStatus = commStatusBool;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(thing).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                // Sign should be correct, but check to make sure.
                meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -double(feedinPower));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridCurrentRChanged, thing, [this, thing](double currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A current (gridCurrentR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridCurrentSChanged, thing, [this, thing](double currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B current (gridCurrentS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridCurrentTChanged, thing, [this, thing](double currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C current (gridCurrentT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridPowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A power (gridPowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseAStateTypeId, double(currentPowerPhaseA));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridPowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B power (gridPowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, double(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, double(currentPowerPhaseC));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridVoltageRChanged, thing, [this, thing](double voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A voltage (gridVoltageR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridVoltageSChanged, thing, [this, thing](double voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B voltage (gridVoltageS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridVoltageTChanged, thing, [this, thing](double voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C voltage (gridVoltageT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::updateFinished, thing, [thing, connection](){
            qCDebug(dcSolax()) << "Solax X3 - Update finished.";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            thing->setStateValue(solaxX3InverterRTUCurrentPowerStateTypeId, -(powerDc1+powerDc2));
        });

        // Battery
        connect(connection, &SolaxModbusRtuConnection::bmsConnectStateChanged, thing, [this, thing](quint16 bmsConnect){
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcSolax()) << "Battery connect state (bmsConnectState) changed" << bmsConnect;
            bool bmsCommStatusBool = (bmsConnect != 0);
            m_batterystates.find(thing)->bmsCommStatus = bmsCommStatusBool;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable) {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                    batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcSolax()) << "Set up Solax battery for" << thing;
                QString name = supportedThings().findById(solaxBatteryThingClassId).displayName();
                ThingDescriptor descriptor(solaxBatteryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::batPowerCharge1Changed, thing, [this, thing](qint16 powerBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery power (batpowerCharge1) changed" << powerBat1 << "W";

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

        connect(connection, &SolaxModbusRtuConnection::batteryCapacityChanged, thing, [this, thing](quint16 socBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";
                batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::bmsWarningLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolax()) << "Battery warning bits LSB recieved" << batteryWarningBitsLsb;
            m_batterystates.find(thing)->bmsWarningLsb = batteryWarningBitsLsb;
            setBmsWarningMessage(thing);
        });

        connect(connection, &SolaxModbusRtuConnection::bmsWarningMsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolax()) << "Battery warning bits MSB recieved" << batteryWarningBitsMsb;
            m_batterystates.find(thing)->bmsWarningMsb = batteryWarningBitsMsb;
            setBmsWarningMessage(thing);
        });

        connect(connection, &SolaxModbusRtuConnection::batVoltageCharge1Changed, thing, [this, thing](double batVoltageCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery voltage changed" << batVoltageCharge1 << "V";
                batteryThings.first()->setStateValue(solaxBatteryBatteryVoltageStateTypeId, batVoltageCharge1);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::batCurrentCharge1Changed, thing, [this, thing](double batCurrentCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery current changed" << batCurrentCharge1 << "A";
                batteryThings.first()->setStateValue(solaxBatteryBatteryCurrentStateTypeId, batCurrentCharge1);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::temperatureBatChanged, thing, [this, thing](quint16 temperatureBat){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery temperature changed" << temperatureBat << "°C";
                batteryThings.first()->setStateValue(solaxBatteryTemperatureStateTypeId, temperatureBat);
            }
        });


        // FIXME: make async and check if this is really an solax
        m_rtuConnections.insert(thing, connection);
        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);
        info->finish(Thing::ThingErrorNoError);
        return;
    }


    if (thing->thingClassId() == solaxMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxX3InverterTCPThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3InverterTCPConnectedStateTypeId).toBool());
            } else if (parentThing->thingClassId() == solaxX3InverterRTUThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3InverterRTUConnectedStateTypeId).toBool());
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
            if (parentThing->thingClassId() == solaxX3InverterTCPThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3InverterTCPThingBatteryCapacityParamTypeId).toUInt());
            } else if (parentThing->thingClassId() == solaxX3InverterRTUThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3InverterRTUThingBatteryCapacityParamTypeId).toUInt());
            }
        }
        m_batteryPowerTimer = new QTimer(this);

        connect(m_batteryPowerTimer, &QTimer::timeout, thing, [this, thing, parentThing]() {
            uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
            int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
            bool forceBattery = thing->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
            qCWarning(dcSolax()) << "Battery countdown timer timeout. Manuel mode is" << forceBattery;
            if (forceBattery) {
                qCWarning(dcSolax()) << "Battery power should be" << powerToSet;
                qCWarning(dcSolax()) << "Battery timeout should be" << batteryTimeout;
                setBatteryPower(parentThing, powerToSet, batteryTimeout, false);
            } else {
                disableRemoteControl(parentThing);
            }
        });

        return;
    }
}

void IntegrationPluginSolax::setupTcpConnection(ThingSetupInfo *info)
{
        qCDebug(dcSolax()) << "Setup TCP connection.";
        Thing *thing = info->thing();
        NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
        uint port = thing->paramValue(solaxX3InverterTCPThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxX3InverterTCPThingModbusIdParamTypeId).toUInt();
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
                qCDebug(dcSolax()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // Reconnect on monitor reachable changed.
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcSolax()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxX3InverterTCPConnectedStateTypeId).toBool()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            } else if (!reachable) {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again.
                connection->disconnectDevice();
            }
        });

        connect(connection, &SolaxModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable){
            qCDebug(dcSolax()) << "Reachable state changed" << reachable;
            if (reachable) {
                // Connected true will be set after successfull init.
                connection->initialize();
            } else {
                thing->setStateValue(solaxX3InverterTCPConnectedStateTypeId, false);
                thing->setStateValue(solaxX3InverterTCPCurrentPowerStateTypeId, 0);
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
            thing->setStateValue(solaxX3InverterTCPConnectedStateTypeId, success);
            setPassword(thing);

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
                qCDebug(dcSolax()) << "Solax inverter initialized.";
                thing->setStateValue(solaxX3InverterTCPFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxX3InverterTCPNominalPowerStateTypeId, connection->inverterType());
                // thing->setStateValue(solaxX3InverterTCPExportLimitStateTypeId, connection->inverterType());
            } else {
                qCDebug(dcSolax()) << "Solax inverter initialization failed.";
                // Try once to reconnect the device
                connection->reconnectDevice();
            } 
        });

        connect(connection, &SolaxModbusTcpConnection::inverterVoltageChanged, thing, [thing](double inverterVoltage){
            qCDebug(dcSolax()) << "Inverter voltage changed" << inverterVoltage << "V";
            thing->setStateValue(solaxX3InverterTCPInverterVoltageStateTypeId, inverterVoltage);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterCurrentChanged, thing, [thing](double inverterCurrent){
            qCDebug(dcSolax()) << "Inverter current changed" << inverterCurrent << "A";
            thing->setStateValue(solaxX3InverterTCPInverterCurrentStateTypeId, inverterCurrent);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc1Changed, thing, [thing](double powerDc1){
            qCDebug(dcSolax()) << "Inverter DC1 power changed" << powerDc1 << "W";
            thing->setStateValue(solaxX3InverterTCPPv1PowerStateTypeId, powerDc1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage1Changed, thing, [thing](double pvVoltage1){
            qCDebug(dcSolax()) << "Inverter PV1 voltage changed" << pvVoltage1 << "V";
            thing->setStateValue(solaxX3InverterTCPPv1VoltageStateTypeId, pvVoltage1);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent1Changed, thing, [thing](double pvCurrent1){
            qCDebug(dcSolax()) << "Inverter PV1 current changed" << pvCurrent1 << "A";
            thing->setStateValue(solaxX3InverterTCPPv1CurrentStateTypeId, pvCurrent1);
        });

        connect(connection, &SolaxModbusTcpConnection::powerDc2Changed, thing, [thing](double powerDc2){
            qCDebug(dcSolax()) << "Inverter DC2 power changed" << powerDc2 << "W";
            thing->setStateValue(solaxX3InverterTCPPv2PowerStateTypeId, powerDc2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvVoltage2Changed, thing, [thing](double pvVoltage2){
            qCDebug(dcSolax()) << "Inverter PV2 voltage changed" << pvVoltage2 << "V";
            thing->setStateValue(solaxX3InverterTCPPv2VoltageStateTypeId, pvVoltage2);
        });

        connect(connection, &SolaxModbusTcpConnection::pvCurrent2Changed, thing, [thing](double pvCurrent2){
            qCDebug(dcSolax()) << "Inverter PV2 current changed" << pvCurrent2 << "A";
            thing->setStateValue(solaxX3InverterTCPPv2CurrentStateTypeId, pvCurrent2);
        });

        connect(connection, &SolaxModbusTcpConnection::runModeChanged, thing, [this, thing](SolaxModbusTcpConnection::RunMode runMode){
            qCDebug(dcSolax()) << "Inverter run mode recieved" << runMode;
            quint16 runModeAsInt = runMode;
            setRunMode(thing, runModeAsInt);
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureChanged, thing, [thing](quint16 temperature){
            qCDebug(dcSolax()) << "Inverter temperature changed" << temperature << "°C";
            thing->setStateValue(solaxX3InverterTCPTemperatureStateTypeId, temperature);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTotalChanged, thing, [thing](double solarEnergyTotal){
            qCDebug(dcSolax()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";
            thing->setStateValue(solaxX3InverterTCPTotalEnergyProducedStateTypeId, solarEnergyTotal);
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolax()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3InverterTCPEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxModbusTcpConnection::readExportLimitChanged, thing, [thing](float limit){
            qCWarning(dcSolax()) << "Export limit changed to " << limit << "W";
            thing->setStateValue(solaxX3InverterTCPExportLimitStateTypeId, limit);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolax()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });

        connect(connection, &SolaxModbusTcpConnection::updateFinished, thing, [this, thing, connection](){
            qCDebug(dcSolax()) << "Solax X3 - Update finished.";
            qint16 inverterPower = connection->inverterPower();
            if (inverterPower < 0)
            {
                inverterPower *= -1;
            }
            qint16 batteryPower = 0;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                batteryPower = connection->batPowerCharge1();
                QString state = batteryThings.first()->stateValue(solaxBatteryChargingStateStateTypeId).toString();
                if (state == "discharging")
                {
                    // Battery is discharging
                    batteryPower = 0;
                }
            }

            qCDebug(dcSolax()) << "Subtract from InverterPower";
            thing->setStateValue(solaxX3InverterTCPCurrentPowerStateTypeId, -inverterPower-batteryPower);
        });

        // Meter
        connect(connection, &SolaxModbusTcpConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            m_meterstates.find(thing)->meterCommStatus = commStatusBool;
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
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
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                // Sign should be correct, but check to make sure.
                meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -double(feedinPower));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentRChanged, thing, [this, thing](double currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A current (gridCurrentR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentSChanged, thing, [this, thing](double currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B current (gridCurrentS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridCurrentTChanged, thing, [this, thing](double currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C current (gridCurrentT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(solaxMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A power (gridPowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseAStateTypeId, double(currentPowerPhaseA));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B power (gridPowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, double(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, double(currentPowerPhaseC));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageRChanged, thing, [this, thing](double voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A voltage (gridVoltageR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageSChanged, thing, [this, thing](double voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B voltage (gridVoltageS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridVoltageTChanged, thing, [this, thing](double voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C voltage (gridVoltageT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(solaxMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyRChanged, thing, [this, thing](double gridFrequencyR){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase A frequency (gridFrequencyR) changed" << gridFrequencyR << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseAStateTypeId, gridFrequencyR);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencySChanged, thing, [this, thing](double gridFrequencyS){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase B frequency (gridFrequencyS) changed" << gridFrequencyS << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseBStateTypeId, gridFrequencyS);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::gridFrequencyTChanged, thing, [this, thing](double gridFrequencyT){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C frequency (gridFrequencyT) changed" << gridFrequencyT << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyPhaseCStateTypeId, gridFrequencyT);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });


        // Battery        
        connect(connection, &SolaxModbusTcpConnection::bmsConnectStateChanged, thing, [this, thing](quint16 bmsConnect){
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcSolax()) << "Battery connect state (bmsConnectState) changed" << bmsConnect;
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
                qCDebug(dcSolax()) << "Set up Solax battery for" << thing;
                QString name = supportedThings().findById(solaxBatteryThingClassId).displayName();
                ThingDescriptor descriptor(solaxBatteryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batPowerCharge1Changed, thing, [this, thing](qint16 powerBat1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery power (batpowerCharge1) changed" << powerBat1 << "W";

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
                qCDebug(dcSolax()) << "Battery state of charge (batteryCapacity) changed" << socBat1 << "%";
                batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningLsbChanged, thing, [this, thing](quint16 batteryWarningBitsLsb){
            qCDebug(dcSolax()) << "Battery warning bits LSB recieved" << batteryWarningBitsLsb;
            m_batterystates.find(thing)->bmsWarningLsb = batteryWarningBitsLsb;
            setBmsWarningMessage(thing);
        });

        connect(connection, &SolaxModbusTcpConnection::bmsWarningMsbChanged, thing, [this, thing](quint16 batteryWarningBitsMsb){
            qCDebug(dcSolax()) << "Battery warning bits MSB recieved" << batteryWarningBitsMsb;
            m_batterystates.find(thing)->bmsWarningMsb = batteryWarningBitsMsb;
            setBmsWarningMessage(thing);
        });

        connect(connection, &SolaxModbusTcpConnection::batVoltageCharge1Changed, thing, [this, thing](double batVoltageCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery voltage changed" << batVoltageCharge1 << "V";
                batteryThings.first()->setStateValue(solaxBatteryBatteryVoltageStateTypeId, batVoltageCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::batCurrentCharge1Changed, thing, [this, thing](double batCurrentCharge1){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery current changed" << batCurrentCharge1 << "A";
                batteryThings.first()->setStateValue(solaxBatteryBatteryCurrentStateTypeId, batCurrentCharge1);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::temperatureBatChanged, thing, [this, thing](quint16 temperatureBat){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcSolax()) << "Battery temperature changed" << temperatureBat << "°C";
                batteryThings.first()->setStateValue(solaxBatteryTemperatureStateTypeId, temperatureBat);
            }
        });


        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

        return;
}

void IntegrationPluginSolax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId || thing->thingClassId() == solaxX3InverterRTUThingClassId) {

        bool connectionWasCreated = m_tcpConnections.contains(thing) || m_rtuConnections.contains(thing);
        if (!connectionWasCreated) {
            qCDebug(dcSolax()) << "Aborting post setup, because setup did not complete.";
            return;
        }

        if (!m_pluginTimer) {
            qCDebug(dcSolax()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(SolaxModbusTcpConnection *connection, m_tcpConnections) {
                    connection->update();
                }

                foreach(SolaxModbusRtuConnection *connection, m_rtuConnections) {
                    // Use this register to test if initialization was done.
                    quint16 ratedPower = connection->inverterType();
                    if (ratedPower < 600) {
                        connection->initialize();
                    } else {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId).isEmpty()) {
            qCDebug(dcSolax()) << "Set up solax meter for" << thing;
            QString name = supportedThings().findById(solaxMeterThingClassId).displayName();
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(solaxMeterThingClassId, name, QString(), thing->id()));
        }
    }

}

void IntegrationPluginSolax::setPassword(Thing *thing)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId)
    {
        qCDebug(dcSolax()) << "Set unlock password";
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
        QModbusReply *reply = connection->setUnlockPassword(2014);
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, thing, [this, thing, reply](){
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcSolax()) << "Error setting unlockpassword" << reply->error() << reply->errorString();
                setPassword(thing);
            } else {
                qCWarning(dcSolax()) << "Successfully set unlock password";
            }
        });

    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        qCDebug(dcSolax()) << "Set unlock password";
    }
}

// Code for setting power limit. Disabled for now.
void IntegrationPluginSolax::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSolax()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxX3InverterTCPSetExportLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3InverterTCPSetExportLimitActionExportLimitParamTypeId).toUInt();
            double ratedPower = thing->stateValue(solaxX3InverterTCPNominalPowerStateTypeId).toDouble();
            qCWarning(dcSolax()) << "Rated power is" << ratedPower;
            qCWarning(dcSolax()) << "Trying to set active power limit to" << powerLimit;
            quint16 target = powerLimit * (ratedPower/100); 
            QModbusReply *reply = connection->setWriteExportLimit(target);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, thing, reply, powerLimit, target](){
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSolax()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCWarning(dcSolax()) << "Active power limit set to" << target;
                    //thing->setStateValue(solaxX3InverterTCPExportLimitStateTypeId, target);
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else if (thing->thingClassId() == solaxBatteryThingClassId) {
        Thing *inverterThing = myThings().findById(thing->parentId());
        qCWarning(dcSolax()) << "executeAction: should be inverter thing" << inverterThing;
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(inverterThing);

        if (!connection) {
            qCWarning(dcSolax()) << "Execute action battery - Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->connected()) {
            qCWarning(dcSolax()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxBatteryEnableForcePowerActionTypeId) {
            bool state = action.paramValue(solaxBatteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
            int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();

            qCWarning(dcSolax()) << "Battery manual mode is enabled?" << state;
            if (state) {
                setBatteryPower(inverterThing, powerToSet, batteryTimeout, false);
            } else {
                disableRemoteControl(inverterThing);
            }

            thing->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, state);
        } else if (action.actionTypeId() == solaxBatteryForcePowerActionTypeId) {
            int batteryPower = action.paramValue(solaxBatteryForcePowerActionForcePowerParamTypeId).toInt();
            qCWarning(dcSolax()) << "Battery power should be set to" << batteryPower;
            thing->setStateValue(solaxBatteryForcePowerStateTypeId, batteryPower);

            uint timeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
            bool state = thing->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
            if (state)
            {
                setBatteryPower(thing, batteryPower, timeout, true);
            }
        } else if (action.actionTypeId() == solaxBatteryForcePowerTimeoutActionTypeId) {
            uint timeout = action.paramValue(solaxBatteryForcePowerTimeoutActionForcePowerTimeoutParamTypeId).toUInt();
            bool forceBattery = thing->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
            qCWarning(dcSolax()) << "Battery timer should be set to" << timeout;
            if (forceBattery && timeout > 0) {
                qCWarning(dcSolax()) << "Battery timer will be set to" << timeout << "as manual mode is enabled";
                if (m_batteryPowerTimer->isActive()) {
                    m_batteryPowerTimer->stop();
                }
                m_batteryPowerTimer->setInterval(timeout*1000);
                m_batteryPowerTimer->start();
            }
            if (timeout > 0) {
                thing->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, timeout);
            }

            int batteryPower = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt(); 
            if (forceBattery)
            {
                setBatteryPower(thing, batteryPower, timeout, true);
            }
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    }
}

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

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (m_meterstates.contains(thing)) {
        m_meterstates.remove(thing);
    }

    if (m_batterystates.contains(thing)) {
        m_batterystates.remove(thing);
    }

    if (m_batteryPowerTimer) {
        if (m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->stop();
        }
        delete m_batteryPowerTimer;
        m_batteryPowerTimer = nullptr;
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
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
        thing->setStateValue(solaxX3InverterTCPInverterStatusStateTypeId, runModeAsString);
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        thing->setStateValue(solaxX3InverterRTUInverterStatusStateTypeId, runModeAsString);
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
    qCDebug(dcSolax()) << errorMessage;
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
        thing->setStateValue(solaxX3InverterTCPErrorMessageStateTypeId, errorMessage);
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        thing->setStateValue(solaxX3InverterRTUErrorMessageStateTypeId, errorMessage);
    }
}

void IntegrationPluginSolax::setBmsWarningMessage(Thing *thing)
{
    Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
    if (!batteryThings.isEmpty()) {
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
        qCDebug(dcSolax()) << warningMessage;
        batteryThings.first()->setStateValue(solaxBatteryWarningMessageStateTypeId, warningMessage);
    }
}


void IntegrationPluginSolax::disableRemoteControl(Thing *thing)
{
    SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
    if (!connection) {
        qCWarning(dcSolax()) << "disableRemoteControl - Modbus connection not available";
        // info->finish(Thing::ThingErrorHardwareFailure);
        return;
    }

    if (!m_batteryPowerTimer->isActive()) {
        m_batteryPowerTimer->stop();
    }
    // TODO: if this is not working, 0x7C needs to be set to 0 instead

    quint32 timeoutValue = (3 << 16) | 3;
    QModbusReply *replyTimeout = connection->setBatteryTimeout(timeoutValue);
    connect(replyTimeout, &QModbusReply::finished, replyTimeout, &QModbusReply::deleteLater);
    connect(replyTimeout, &QModbusReply::finished, thing, [thing, replyTimeout](){
        if (replyTimeout->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "disableRemoteControl - Error setting timeout" << replyTimeout->error() << replyTimeout->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "disableRemoteControl - Timeout set to 3";
            //info->finish(Thing::ThingErrorNoError);
        }
    });

    // quint32 modeTypeValue = (1 << 16) | 8;
    quint32 modeTypeValue = (8 << 16) | 1;
    QModbusReply *replyMode = connection->setModeType(modeTypeValue);
    connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
    connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode](){
        if (replyMode->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "disableRemoteControl - Error setting mode and type" << replyMode->error() << replyMode->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "disableRemoteControl - Mode set to 8, Type set to 1";
            //info->finish(Thing::ThingErrorNoError);
        }
    });
}

void IntegrationPluginSolax::setBatteryPower(Thing *thing, qint32 powerToSet, quint16 batteryTimeout, bool update)
{
    SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
    if (!connection) {
        qCWarning(dcSolax()) << "setBatteryPower - Modbus connection not available";
        // info->finish(Thing::ThingErrorHardwareFailure);
        return;
    }

    if (!m_batteryPowerTimer->isActive()) {
        m_batteryPowerTimer->setInterval(batteryTimeout*1000);
        m_batteryPowerTimer->start();
    }

    quint32 pvPower = thing->stateValue(solaxX3InverterTCPNominalPowerStateTypeId).toUInt();
    QModbusReply *replyPvPower = connection->setPvPowerLimit(pvPower);
    connect(replyPvPower, &QModbusReply::finished, replyPvPower, &QModbusReply::deleteLater);
    connect(replyPvPower, &QModbusReply::finished, thing, [thing, replyPvPower, pvPower](){
        if (replyPvPower->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "setBatteryPower - Error setting pv power limit" << replyPvPower->error() << replyPvPower->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "setBatteryPower - Set pv power limit to maximum" << pvPower;
            //info->finish(Thing::ThingErrorNoError);
        }
    });

    QModbusReply *replyBatterPower = connection->setForceBatteryPower(powerToSet);
    connect(replyBatterPower, &QModbusReply::finished, replyBatterPower, &QModbusReply::deleteLater);
    connect(replyBatterPower, &QModbusReply::finished, thing, [thing, replyBatterPower, powerToSet](){
        if (replyBatterPower->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "setBatteryPower - Error setting pv power limit" << replyBatterPower->error() << replyBatterPower->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "setBatteryPower - Set battery power to" << powerToSet;
            //info->finish(Thing::ThingErrorNoError);
        }
    });

    //quint32 timeoutValue = (batteryTimeout << 16) | (batteryTimeout+5);
    quint32 timeoutValue = ((batteryTimeout+5) << 16) | (batteryTimeout);
    QModbusReply *replyTimeout = connection->setBatteryTimeout(timeoutValue);
    connect(replyTimeout, &QModbusReply::finished, replyTimeout, &QModbusReply::deleteLater);
    connect(replyTimeout, &QModbusReply::finished, thing, [thing, replyTimeout](){
        if (replyTimeout->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "setBatteryPower - Error setting battery timeout" << replyTimeout->error() << replyTimeout->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "setBatteryPower - Successfully set battery timeout";
            //info->finish(Thing::ThingErrorNoError);
        }
    });

    quint8 setType = 0;
    if (update) {
        setType = 2;
    } else {
        setType = 1;
    }
    // quint32 modeTypeValue = (setType << 16) | 8;
    quint32 modeTypeValue = (8 << 16) | setType;
    QModbusReply *replyMode = connection->setModeType(modeTypeValue);
    connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
    connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode](){
        if (replyMode->error() != QModbusDevice::NoError) {
            qCWarning(dcSolax()) << "setBatteryPower - Error setting mode and type" << replyMode->error() << replyMode->errorString();
            //info->finish(Thing::ThingErrorHardwareFailure);
        } else {
            qCWarning(dcSolax()) << "setBatteryPower - Mode set to 8, Type set to 1";
            //info->finish(Thing::ThingErrorNoError);
        }
    });
}
