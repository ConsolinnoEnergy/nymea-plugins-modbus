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
#include "discoverytcpevcg2.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QEventLoop>
#include <QtMath>

QString evcG2StateToString(SolaxEvcG2ModbusTcpConnection::State state)
{
    auto stateStr = QString{};
    switch (state) {
        case SolaxEvcG2ModbusTcpConnection::StateAvailable:
            stateStr = "Available";
            break;
        case SolaxEvcG2ModbusTcpConnection::StatePreparing:
            stateStr = "Preparing";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateCharging:
            stateStr = "Charging";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateFinish:
            stateStr = "Finish";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateFaulted:
            stateStr = "Faulted";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateUnavailable:
            stateStr = "Unavailable";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateReserved:
            stateStr = "Reserved";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateSuspendedEV:
            stateStr = "Suspended EV";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateSuspendedEVSE:
            stateStr = "Suspended EVSE";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateUpdate:
            stateStr = "Update";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateCardActivation:
            stateStr = "Card Activation";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateStartDelay:
            stateStr = "Start Delay";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateChargePause:
            stateStr = "Charge Pause";
            break;
        case SolaxEvcG2ModbusTcpConnection::StateStopping:
            stateStr = "Stopping";
            break;
        default:
            stateStr = "Unknown";
            break;
    }
    return stateStr;
}

quint16 chargePhaseToPhaseCount(SolaxEvcG2ModbusTcpConnection::ChargePhase chargePhase)
{
    return (chargePhase == SolaxEvcG2ModbusTcpConnection::ChargePhaseThreePhase) ? 3 : 1;
}

float typePowerToMaxChargeCurrent(SolaxEvcG2ModbusTcpConnection::TypePower typePower)
{
    auto maxChargeCurrent = 16.f;
    switch (typePower) {
        case SolaxEvcG2ModbusTcpConnection::TypePower6kW:
            maxChargeCurrent = 16.f;
            break;
        case SolaxEvcG2ModbusTcpConnection::TypePower7kW:
            maxChargeCurrent = 32.f;
            break;
        case SolaxEvcG2ModbusTcpConnection::TypePower11kW:
            maxChargeCurrent = 16.f;
            break;
        case SolaxEvcG2ModbusTcpConnection::TypePower22kW:
            maxChargeCurrent = 32.f;
            break;
        case SolaxEvcG2ModbusTcpConnection::TypePower4p6kW:
            maxChargeCurrent = 20.f;
            break;
        }
    return maxChargeCurrent;
}

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

                // Set connected state for meter 2
                Things meter2Things = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
                if (!meter2Things.isEmpty()) {
                    meter2Things.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
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

    // Create error logging file
    QFile errorCounterFile("/data/solax-counter.txt");
    if (!errorCounterFile.exists()) {
        errorCounterFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QByteArray counter("Init: 0\nCont: 0");
        errorCounterFile.write(counter);
    }
    errorCounterFile.close();
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
    } else if (info->thingClassId() == solaxEvcG2ThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolax()) << "The network discovery is not available on this platform.";
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
                qCInfo(dcSolax()) << "Discovered:" << descriptor.title() << descriptor.description();

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

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        SolaxModbusRtuConnection *connection = new SolaxModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &SolaxModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(solaxX3InverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->modbusReachable = reachable;
                if (reachable && m_meterstates.value(meterThings.first()).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }
            }
            
            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!secondaryMeterThings.isEmpty()) {
                m_meterstates.find(secondaryMeterThings.first())->modbusReachable = reachable;
                if (reachable && m_meterstates.value(secondaryMeterThings.first()).meterCommStatus) {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, 0);
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
                    batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
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
                m_energyCheck = 500;
                thing->setStateValue(solaxX3InverterRTUFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxX3InverterRTUNominalPowerStateTypeId, connection->inverterType());
                writePasswordToInverter(thing);
                // thing->setStateValue(solaxX3InverterRTUExportLimitStateTypeId, connection->inverterType());
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

        connect(connection, &SolaxModbusRtuConnection::solarEnergyTotalChanged, thing, [this, thing](double solarEnergyTotal){
            qCDebug(dcSolax()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            double oldEnergyValue = thing->stateValue(solaxX3InverterRTUTotalEnergyProducedStateTypeId).toDouble();
            double diffEnergy = solarEnergyTotal - oldEnergyValue;
            if (oldEnergyValue == 0 ||
                (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
            {
                thing->setStateValue(solaxX3InverterRTUTotalEnergyProducedStateTypeId, solarEnergyTotal);
            } else {
                qCWarning(dcSolax()) << "RTU Inverter - Old Energy value is" << oldEnergyValue;
                qCWarning(dcSolax()) << "RTU Inverter - New energy value is" << solarEnergyTotal;
                writeErrorLog();
            }
        });

        connect(connection, &SolaxModbusRtuConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolax()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3InverterRTUEnergyProducedTodayStateTypeId, solarEnergyToday);
        });


        // Meter
        connect(connection, &SolaxModbusRtuConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::meter2CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            // Get max power of the inverter
            double nominalPowerInverter = thing->stateValue(solaxX3InverterRTUNominalPowerStateTypeId).toDouble();
            double nominalPowerBattery = 0;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                // Get max power of the battery
                nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
            }

            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                if (qFabs(feedinPower) < (nominalPowerInverter + nominalPowerBattery + 1000))
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -1 * static_cast<double>(feedinPower));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "RTU Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "RTU Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "RTU Meter Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "RTU Meter Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
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
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseBStateTypeId, static_cast<double>(currentPowerPhaseB));
            }
        });

        connect(connection, &SolaxModbusRtuConnection::gridPowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter phase C power (gridPowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(solaxMeterCurrentPowerPhaseCStateTypeId, static_cast<double>(currentPowerPhaseC));
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
        connect(connection, &SolaxModbusRtuConnection::readExportLimitChanged, thing, [thing](float limit){
            qCWarning(dcSolax()) << "Export limit changed to " << limit << "W";
            thing->setStateValue(solaxX3InverterRTUExportLimitStateTypeId, limit);
        });

        connect(connection, &SolaxModbusRtuConnection::inverterFrequencyChanged, thing, [this, thing](double frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter frequency (gridFrequency) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(solaxMeterFrequencyStateTypeId, frequency);
            }
        });

        connect(connection, &SolaxModbusRtuConnection::modbusPowerControlChanged, thing, [this, thing](quint16 controlMode) {
            qCWarning(dcSolax()) << "Modbus power control changed to" << controlMode;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (controlMode == 4) {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::updateFinished, thing, [this, thing, connection](){
            qCDebug(dcSolax()) << "Solax X3 - Update finished.";

            // TODO: does this reliably create the secondary meter?
            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);

            quint16 meter2CommState = connection->meter2CommunicationState();
            if (meter2CommState == 1 && meter2CommState == m_secondMeterCheck) {
                m_secondMeterCounter++;
            } else {
                m_secondMeterCounter = 0;
            }
            m_secondMeterCheck = meter2CommState;

            if (secondaryMeterThings.isEmpty() && m_secondMeterCounter == 3) {
                qCDebug(dcSolax()) << "Create the secondary meter thing";
                QString name = supportedThings().findById(solaxMeterSecondaryThingClassId).displayName();
                ThingDescriptor descriptor(solaxMeterSecondaryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            qint16 currentBatPower = 0;
            if (!batteryThings.isEmpty()) {
                currentBatPower = batteryThings.first()->stateValue(solaxBatteryCurrentPowerStateTypeId).toInt();
                if (m_batteryPowerTimer->isActive()) {
                    batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, (int) m_batteryPowerTimer->remainingTime()/1000);
                }

                float batMaxVoltage = connection->batMaxDischargeVolt();
                float batMaxCurrent = connection->batMaxDischargeCurrent();
                double batMaxPower = batMaxCurrent*batMaxVoltage;
                // It seems batMaxVoltage and matMaxCurrent are not always provided by the system
                if (batMaxPower == 0)
                    batMaxPower = 10000;

                batteryThings.first()->setStateValue(solaxBatteryNominalPowerBatteryStateTypeId, batMaxPower);

                // check if manual is following the chosen setting
                bool userSetState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
                bool actualState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (userSetState != actualState) {
                    if (userSetState == true) {
                        uint batteryTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
                        int powerToSet = batteryThings.first()->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
                        setBatteryPower(thing, powerToSet, batteryTimeout);
                    } else {
                        disableRemoteControl(thing);
                    }
                }
            }

            qCDebug(dcSolax()) << "Set inverter power";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            // If battery is currently charging, check if batteryPower is bigger than solaxpower
            // If yes, add that difference to solarpower
            qint16 powerDifference = 0;
            if (currentBatPower > 0 && (powerDc1+powerDc2) > 0) {
                powerDifference = currentBatPower - (powerDc1+powerDc2);
                if (powerDifference < 0) {
                    powerDifference = 0;
                }
            }
            double nominalPowerInverter = thing->stateValue(solaxX3InverterRTUNominalPowerStateTypeId).toDouble();
            double currentPower = powerDc1+powerDc2+2*powerDifference;
            if (qFabs(currentPower) < nominalPowerInverter + 5000)
                thing->setStateValue(solaxX3InverterRTUCurrentPowerStateTypeId, -(powerDc1+powerDc2+powerDifference/10));

            m_energyCheck = 10;
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

                double nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
                if (qFabs(powerBat1) < nominalPowerBattery + 5000)
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

                quint16 oldBatteryLevel = batteryThings.first()->stateValue(solaxBatteryBatteryLevelStateTypeId).toUInt();
                quint16 batteryLevelDiff = qFabs(socBat1-oldBatteryLevel);
                if (oldBatteryLevel == 0 || batteryLevelDiff <= 10)
                    batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);

                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
                int minBatteryLevel = batteryThings.first()->stateValue(solaxBatteryMinBatteryLevelStateTypeId).toInt();
                bool batManualMode = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (socBat1 <= minBatteryLevel && batManualMode == true) {
                    qCWarning(dcSolax()) << "Batter level below set minimum value";
                    disableRemoteControl(thing);
                    // batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateTypeId, false);
                }
            }
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

        // Set meter 2 states
        connect(connection, &SolaxModbusRtuConnection::consumEnergyTotalMeter2Changed, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId, 0);
                } else {
                    qCWarning(dcSolax()) << "RTU Meter 2 Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "RTU Meter 2 Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::feedinEnergyTotalMeter2Changed, thing, [this, thing](double feedinEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "RTU Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "RTU Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusRtuConnection::currentPowerMeter2Changed, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                double power = qFabs(static_cast<double>(feedinPower));
                if (power <= 20000)
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, -1 * power);
            }
        });

        // FIXME: make async and check if this is really an solax
        m_rtuConnections.insert(thing, connection);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);
        info->finish(Thing::ThingErrorNoError);
        return;
    }


    if (thing->thingClassId() == solaxMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxX3InverterTCPThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3InverterTCPConnectedStateTypeId).toBool());
            } else if (parentThing->thingClassId() == solaxX3InverterRTUThingClassId) {
                thing->setStateValue(solaxMeterConnectedStateTypeId, parentThing->stateValue(solaxX3InverterRTUConnectedStateTypeId).toBool());
            }
        }

        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        return;
    }

    if (thing->thingClassId() == solaxMeterSecondaryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            if (parentThing->thingClassId() == solaxX3InverterTCPThingClassId) {
                thing->setStateValue(solaxMeterSecondaryConnectedStateTypeId, parentThing->stateValue(solaxX3InverterTCPConnectedStateTypeId).toBool());
            } else if (parentThing->thingClassId() == solaxX3InverterRTUThingClassId) {
                thing->setStateValue(solaxMeterSecondaryConnectedStateTypeId, parentThing->stateValue(solaxX3InverterRTUConnectedStateTypeId).toBool());
            }
        }

        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
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
            qCWarning(dcSolax()) << "Setting up Battery Timer";
            m_batteryPowerTimer = new QTimer(this);

            connect(m_batteryPowerTimer, &QTimer::timeout, thing, [this, thing, parentThing]() {
                uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
                int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
                bool forceBattery = thing->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
                qCWarning(dcSolax()) << "Battery countdown timer timeout. Manuel mode is" << forceBattery;
                if (forceBattery) {
                    qCWarning(dcSolax()) << "Battery power should be" << powerToSet;
                    qCWarning(dcSolax()) << "Battery timeout should be" << batteryTimeout;
                    setBatteryPower(parentThing, powerToSet, batteryTimeout);
                } else {
                    disableRemoteControl(parentThing);
                }
            });
        }

        return;
    }

    if (thing->thingClassId() == solaxEvcG2ThingClassId) {
        // Handle reconfigure
        if (m_evcG2TcpConnections.contains(thing)) {
            qCDebug(dcSolax()) << "Already have a Solax connection for this thing. Cleaning up "
                                  "old connection and initializing new one...";
            SolaxEvcG2ModbusTcpConnection *connection = m_evcG2TcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolax()) << "Setting up a new device: " << thing->params();
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
            qCWarning(dcSolax())
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
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
            // Clean up in case the setup gets aborted.
            if (m_monitors.contains(thing)) {
                qCDebug(dcSolax()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });
        setupEvcG2TcpConnection(info);
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
                    childThing->setStateValue("currentPower", 0);
                }

                // Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
                // if (!meterThings.isEmpty()) {
                //     meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                //     meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                // }

                // Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
                // if (!batteryThings.isEmpty()) {
                //     batteryThings.first()->setStateValue(solaxBatteryCurrentPowerStateTypeId, 0);
                //     batteryThings.first()->setStateValue(solaxBatteryConnectedStateTypeId, false);
                // }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::initializationFinished, thing, [=](bool success){
            thing->setStateValue(solaxX3InverterTCPConnectedStateTypeId, success);
            m_energyCheck = 500;
            writePasswordToInverter(thing);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->modbusReachable = success;
                if (success && m_meterstates.value(meterThings.first()).meterCommStatus) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                }
            }

            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!secondaryMeterThings.isEmpty()) {
                m_meterstates.find(secondaryMeterThings.first())->modbusReachable = success;
                if (success && m_meterstates.value(secondaryMeterThings.first()).meterCommStatus) {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    secondaryMeterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
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

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTotalChanged, thing, [this, thing](double solarEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            qCDebug(dcSolax()) << "Inverter solar energy total changed" << solarEnergyTotal << "kWh";

            double oldEnergyValue = thing->stateValue(solaxX3InverterTCPTotalEnergyProducedStateTypeId).toDouble();
            double diffEnergy = solarEnergyTotal - oldEnergyValue;
            if (oldEnergyValue == 0 ||
                (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
            {
                thing->setStateValue(solaxX3InverterTCPTotalEnergyProducedStateTypeId, solarEnergyTotal);
            } else {
                qCWarning(dcSolax()) << "TCP Inverter - Old Energy value is" << oldEnergyValue;
                qCWarning(dcSolax()) << "TCP Inverter - New energy value is" << solarEnergyTotal;
                writeErrorLog();
            }
        });

        connect(connection, &SolaxModbusTcpConnection::solarEnergyTodayChanged, thing, [thing](double solarEnergyToday){
            qCDebug(dcSolax()) << "Inverter solar energy today changed" << solarEnergyToday << "kWh";
            thing->setStateValue(solaxX3InverterTCPEnergyProducedTodayStateTypeId, solarEnergyToday);
        });

        connect(connection, &SolaxModbusTcpConnection::readExportLimitChanged, thing, [thing](float limit){
            qCWarning(dcSolax()) << "Export limit changed to " << limit << "W";
            thing->setStateValue(solaxX3InverterTCPExportLimitStateTypeId, limit);
        });

        connect(connection, &SolaxModbusTcpConnection::modbusPowerControlChanged, thing, [this, thing](quint16 controlMode) {
            qCWarning(dcSolax()) << "Modbus power control changed to" << controlMode;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (controlMode == 4) {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::updateFinished, thing, [this, thing, connection](){
            qCDebug(dcSolax()) << "Solax X3 - Update finished.";

            // TODO: does this reliably create the secondary meter?
            Things secondaryMeterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);

            quint16 meter2CommState = connection->meter2CommunicationState();
            if (meter2CommState == 1 && meter2CommState == m_secondMeterCheck) {
                m_secondMeterCounter++;
            } else {
                m_secondMeterCounter = 0;
            }
            m_secondMeterCheck = meter2CommState;

            if (secondaryMeterThings.isEmpty() && m_secondMeterCounter == 3) {
                qCDebug(dcSolax()) << "Create the secondary meter thing";
                QString name = supportedThings().findById(solaxMeterSecondaryThingClassId).displayName();
                ThingDescriptor descriptor(solaxMeterSecondaryThingClassId, name, QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }

            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            qint16 currentBatPower = 0;
            if (!batteryThings.isEmpty()) {
                currentBatPower = batteryThings.first()->stateValue(solaxBatteryCurrentPowerStateTypeId).toInt();
                if (m_batteryPowerTimer->isActive()) {
                    batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, (int) m_batteryPowerTimer->remainingTime()/1000);
                }

                float batMaxVoltage = connection->batMaxDischargeVolt();
                float batMaxCurrent = connection->batMaxDischargeCurrent();
                double batMaxPower = batMaxCurrent*batMaxVoltage;
                // It seems batMaxVoltage and matMaxCurrent are not always provided by the system
                if (batMaxPower == 0)
                    batMaxPower = 10000;

                batteryThings.first()->setStateValue(solaxBatteryNominalPowerBatteryStateTypeId, batMaxPower);

                // check if manual is following the chosen setting
                bool userSetState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateTypeId).toBool();
                bool actualState = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (userSetState != actualState) {
                    if (userSetState == true) {
                        uint batteryTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
                        int powerToSet = batteryThings.first()->stateValue(solaxBatteryForcePowerStateTypeId).toInt();
                        setBatteryPower(thing, powerToSet, batteryTimeout);
                    } else {
                        disableRemoteControl(thing);
                    }
                }
            }

            qCDebug(dcSolax()) << "Set inverter power";
            quint16 powerDc1 = connection->powerDc1();
            quint16 powerDc2 = connection->powerDc2();
            // If battery is currently charging, check if batteryPower is bigger than solaxpower
            // If yes, add that difference to solarpower
            qint16 powerDifference = 0;
            if (currentBatPower > 0 && (powerDc1+powerDc2) > 0) {
                powerDifference = currentBatPower - (powerDc1+powerDc2);
                if (powerDifference < 0) {
                    powerDifference = 0;
                }
            }
            double nominalPowerInverter = thing->stateValue(solaxX3InverterTCPNominalPowerStateTypeId).toDouble();
            double currentPower = powerDc1+powerDc2+2*powerDifference;
            if (qFabs(currentPower) < nominalPowerInverter + 5000)
                thing->setStateValue(solaxX3InverterTCPCurrentPowerStateTypeId, -(powerDc1+powerDc2+powerDifference/10));

            m_energyCheck = 10;
        });

        // Meter
        connect(connection, &SolaxModbusTcpConnection::meter1CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::meter2CommunicationStateChanged, thing, [this, thing](quint16 commStatus){
            bool commStatusBool = (commStatus != 0);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(meterThings.first())->meterCommStatus = commStatusBool;
                qCDebug(dcSolax()) << "Meter comm status changed" << commStatus;
                if (commStatusBool && m_meterstates.value(meterThings.first()).modbusReachable) {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(solaxMeterSecondaryConnectedStateTypeId, false);
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, 0);
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinPowerChanged, thing, [this, thing](qint32 feedinPower){
            double nominalPowerInverter = thing->stateValue(solaxX3InverterTCPNominalPowerStateTypeId).toDouble();
            double nominalPowerBattery = 0;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                // Get max power of the battery
                nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
            }

            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                if (qFabs(feedinPower) < (nominalPowerInverter + nominalPowerBattery + 1000))
                    meterThings.first()->setStateValue(solaxMeterCurrentPowerStateTypeId, -1 * static_cast<double>(feedinPower));
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinEnergyTotalChanged, thing, [this, thing](double feedinEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "TCP Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "TCP Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::consumEnergyTotalChanged, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterTotalEnergyConsumedStateTypeId, consumEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "TCP Meter Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "TCP Meter Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
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

                double nominalPowerBattery = batteryThings.first()->stateValue(solaxBatteryNominalPowerBatteryStateTypeId).toDouble();
                if (qFabs(powerBat1) < nominalPowerBattery + 5000)
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

                quint16 oldBatteryLevel = batteryThings.first()->stateValue(solaxBatteryBatteryLevelStateTypeId).toUInt();
                quint16 batteryLevelDiff = qFabs(socBat1-oldBatteryLevel);
                if (oldBatteryLevel == 0 || batteryLevelDiff <= 10)
                    batteryThings.first()->setStateValue(solaxBatteryBatteryLevelStateTypeId, socBat1);

                batteryThings.first()->setStateValue(solaxBatteryBatteryCriticalStateTypeId, socBat1 < 10);
                int minBatteryLevel = batteryThings.first()->stateValue(solaxBatteryMinBatteryLevelStateTypeId).toInt();
                bool batManualMode = batteryThings.first()->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
                if (socBat1 <= minBatteryLevel && batManualMode == true) {
                    qCWarning(dcSolax()) << "Batter level below set minimum value";
                    disableRemoteControl(thing);
                    // batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, false);
                    batteryThings.first()->setStateValue(solaxBatteryEnableForcePowerStateTypeId, false);
                }
            }
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

        // Set meter 2 states
        connect(connection, &SolaxModbusTcpConnection::consumEnergyTotalMeter2Changed, thing, [this, thing](double consumEnergyTotal){
            // New value should not be smaller then old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter consumed energy total (consumEnergyTotal) changed" << consumEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId).toDouble();
                double diffEnergy = consumEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyConsumedStateTypeId, 0);
                } else {
                    qCWarning(dcSolax()) << "TCP Meter 2 Consumed - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "TCP Meter 2 Consumed - New energy value is" << consumEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::feedinEnergyTotalMeter2Changed, thing, [this, thing](double feedinEnergyTotal){
            // New value should not be smaller than the old one.
            // Difference should not be greater than 10
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter exported energy total (feedinEnergyTotal) changed" << feedinEnergyTotal << "kWh";
                double oldEnergyValue = meterThings.first()->stateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId).toDouble();
                double diffEnergy = feedinEnergyTotal - oldEnergyValue;
                if (oldEnergyValue == 0 ||
                    (diffEnergy >= 0 && diffEnergy <= m_energyCheck))
                {
                    meterThings.first()->setStateValue(solaxMeterSecondaryTotalEnergyProducedStateTypeId, feedinEnergyTotal);
                } else {
                    qCWarning(dcSolax()) << "TCP Meter Produced - Old Energy value is" << oldEnergyValue;
                    qCWarning(dcSolax()) << "TCP Meter Produced - New energy value is" << feedinEnergyTotal;
                    writeErrorLog();
                }
            }
        });

        connect(connection, &SolaxModbusTcpConnection::currentPowerMeter2Changed, thing, [this, thing](qint32 feedinPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterSecondaryThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSolax()) << "Meter power (feedin_power, power exported to grid) changed" << feedinPower << "W";
                double power = qFabs(static_cast<double>(feedinPower));
                if (power <= 20000)
                    meterThings.first()->setStateValue(solaxMeterSecondaryCurrentPowerStateTypeId, -1 * power);
            }
        });

        if (monitor->reachable())
            connection->connectDevice();

        info->finish(Thing::ThingErrorNoError);

        return;
}

void IntegrationPluginSolax::setupEvcG2TcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcSolax()) << "Setup EVC G2 TCP connection.";
    const auto thing = info->thing();
    const auto monitor = m_monitors.value(thing);
    uint port = thing->paramValue(solaxEvcG2ThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(solaxEvcG2ThingModbusIdParamTypeId).toUInt();
    const auto connection = new SolaxEvcG2ModbusTcpConnection {
            monitor->networkDeviceInfo().address(),
            port,
            modbusId,
            this };
    m_evcG2TcpConnections.insert(thing, connection);

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcSolax()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(solaxEvcG2ConnectedStateTypeId).toBool()) {
            connection->modbusTcpMaster()->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::reachableChanged, thing,
            [connection, thing](bool reachable) {
                qCDebug(dcSolax()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(solaxEvcG2ConnectedStateTypeId, false);
                    thing->setStateValue(solaxEvcG2CurrentPowerStateTypeId, 0);
                }
            });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::initializationFinished, thing,
            [thing, connection, this](bool success) {
                thing->setStateValue(solaxEvcG2ConnectedStateTypeId, success);

                if (success) {
                    qCDebug(dcSolax()) << "Solax wallbox initialized.";
                    thing->setStateValue(solaxEvcG2FirmwareVersionStateTypeId,
                                         connection->firmwareVersion());
                    const auto maxChargeCurrent =
                            typePowerToMaxChargeCurrent(connection->typePower());
                    qCDebug(dcSolax())
                            << "Setting maximum value of max. charge current to"
                            << maxChargeCurrent;
                    thing->setStateMaxValue(solaxEvcG2MaxChargingCurrentStateTypeId,
                                            maxChargeCurrent);
                    configureEvcG2(connection);
                } else {
                    qCDebug(dcSolax()) << "Solax wallbox initialization failed.";
                    // Try to reconnect to device
                    connection->reconnectDevice();
                }
            });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::typePowerChanged, thing,
            [thing](SolaxEvcG2ModbusTcpConnection::TypePower typePower) {
                qCDebug(dcSolax()) << "Received wallbox power type information:" << typePower;
                const auto maxChargeCurrent = typePowerToMaxChargeCurrent(typePower);
                qCDebug(dcSolax())
                        << "Setting maximum value of max. charge current to"
                        << maxChargeCurrent;
                thing->setStateMaxValue(solaxEvcG2MaxChargingCurrentStateTypeId,
                                        maxChargeCurrent);
            });

    connect(connection, &SolaxEvcG2ModbusTcpConnection::stateChanged, thing,
            [this, thing, connection](SolaxEvcG2ModbusTcpConnection::State state) {
                thing->setStateValue(solaxEvcG2StateStateTypeId, evcG2StateToString(state));
                thing->setStateValue(solaxEvcG2ChargingStateTypeId,
                                     state == SolaxEvcG2ModbusTcpConnection::StateCharging);
                if (state == SolaxEvcG2ModbusTcpConnection::StateAvailable ||
                        state == SolaxEvcG2ModbusTcpConnection::StateUnavailable ||
                        state == SolaxEvcG2ModbusTcpConnection::StateUpdate) {
                    thing->setStateValue(solaxEvcG2PluggedInStateTypeId, false);
                    thing->setStateValue(solaxEvcG2PhaseCountStateTypeId, 0);
                } else {
                    thing->setStateValue(solaxEvcG2PluggedInStateTypeId, true);
                    thing->setStateValue(solaxEvcG2PhaseCountStateTypeId,
                                         chargePhaseToPhaseCount(connection->chargePhase()));
                }

                // Make wallbox follow our charging instructions.
                if (state == SolaxEvcG2ModbusTcpConnection::StateCharging) {
                    if (!thing->stateValue(solaxEvcG2PowerStateTypeId).toBool()) {
                        qCDebug(dcSolax()) << "EV charging but instructed not to. Disable charging...";
                        setEvcG2Charging(connection, false);
                    }
                } else {
                    if (thing->stateValue(solaxEvcG2PluggedInStateTypeId).toBool() &&
                            thing->stateValue(solaxEvcG2PowerStateTypeId).toBool()) {
                        qCDebug(dcSolax()) << "EV plugged in and instructed to charge. Enable charging...";
                        setEvcG2Charging(connection, true);
                    }
                }
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::faultCodeChanged, thing,
            [thing](quint32 faultCode) {
                auto msg = QString{};
                switch (faultCode) {
                    case 0x0:
                        msg = "No Error";
                        break;
                    case 0x1:
                        msg = "Emergency Stop Fault";
                        break;
                    case 0x2:
                        msg = "Overcurrent Fault";
                        break;
                    case 0x4:
                        msg = "Temperature beyond limit";
                        break;
                    case 0x8:
                        msg = "PE grounding fault";
                        break;
                    case 0x10:
                        msg = "6mA leakage current fault";
                        break;
                    case 0x20:
                        msg = "PE leakage current fault";
                        break;
                    case 0x40:
                        msg = "Over power fault";
                        break;
                    case 0x100:
                        msg = "L1 phase overvoltage fault";
                        break;
                    case 0x200:
                        msg = "L1 phase undervoltage fault";
                        break;
                    case 0x400:
                        msg = "L2 phase overvoltage fault";
                        break;
                    case 0x800:
                        msg = "L2 phase undervoltage fault";
                        break;
                    case 0x1000:
                        msg = "L3 phase overvoltage fault";
                        break;
                    case 0x2000:
                        msg = "L3 phase undervoltage fault";
                        break;
                    case 0x4000:
                        msg = "Metering chip communication fault";
                        break;
                    case 0x8000:
                        msg = "RS485 communication fault";
                        break;
                    case 0x10000:
                        msg = "Power selection fault";
                        break;
                    case 0x20000:
                        msg = "CP voltage fault";
                        break;
                    case 0x40000:
                        msg = "Electronic lock fault";
                        break;
                    case 0x80000:
                        msg = "Meter type fault";
                        break;
                    case 0x100000:
                        msg = "EV-Charger tampered alarm";
                        break;
                    case 0x200000:
                        msg = "PEN relay fault";
                        break;
                    case 0x400000:
                        msg = "Parallel communication fault";
                        break;
                    case 0x800000:
                        msg = "First relay welding detection fault";
                        break;
                    case 0x1000000:
                        msg = "First relay malfunction fault";
                        break;
                    case 0x2000000:
                        msg = "Second relay welding detection fault";
                        break;
                    case 0x4000000:
                        msg = "Second relay malfunction fault";
                        break;
                    default:
                        msg = QString::number(faultCode, 16);
                }

                thing->setStateValue(solaxEvcG2FaultCodeStateTypeId, msg);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::totalPowerChanged, thing,
            [thing](float totalPower) {
                thing->setStateValue(solaxEvcG2CurrentPowerStateTypeId, totalPower);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::sessionEnergyChanged, thing,
            [thing](float sessionEnergy) {
                thing->setStateValue(solaxEvcG2SessionEnergyStateTypeId, sessionEnergy);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::totalEnergyChanged, thing,
            [thing](float totalEnergy) {
                thing->setStateValue(solaxEvcG2TotalEnergyConsumedStateTypeId, totalEnergy);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::typePhaseChanged, thing,
            [thing](SolaxEvcG2ModbusTcpConnection::TypePhase typePhase) {
                switch (typePhase) {
                    case SolaxEvcG2ModbusTcpConnection::TypePhaseSinglePhase:
                        thing->setStateMaxValue(solaxEvcG2PhaseCountStateTypeId, 1);
                        break;
                    case SolaxEvcG2ModbusTcpConnection::TypePhaseThreePhase:
                        thing->setStateMaxValue(solaxEvcG2PhaseCountStateTypeId, 3);
                        break;
                }
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::chargePhaseChanged, thing,
            [thing](SolaxEvcG2ModbusTcpConnection::ChargePhase chargePhase) {
                thing->setStateValue(solaxEvcG2PhaseCountStateTypeId,
                                     chargePhaseToPhaseCount(chargePhase));
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::chargingTimeChanged, thing,
            [thing](quint32 chargingTime) {
                thing->setStateValue(solaxEvcG2ChargingTimeStateTypeId, chargingTime);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::currentPhaseAChanged, thing,
            [thing](float currentPhaseA) {
                thing->setStateValue(solaxEvcG2CurrentPhaseAStateTypeId, currentPhaseA);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::currentPhaseBChanged, thing,
            [thing](float currentPhaseB) {
                thing->setStateValue(solaxEvcG2CurrentPhaseBStateTypeId, currentPhaseB);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::currentPhaseCChanged, thing,
            [thing](float currentPhaseC) {
                thing->setStateValue(solaxEvcG2CurrentPhaseCStateTypeId, currentPhaseC);
            });
    connect(connection, &SolaxEvcG2ModbusTcpConnection::maxChargeCurrentChanged, thing,
            [thing](float maxChargeCurrent) {
                thing->setStateValue(solaxEvcG2MaxChargingCurrentStateTypeId, maxChargeCurrent);
            });

    if (monitor->reachable()) {
        connection->connectDevice();
    }
    info->finish(Thing::ThingErrorNoError);
    return;
}

void IntegrationPluginSolax::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId ||
            thing->thingClassId() == solaxX3InverterRTUThingClassId ||
            thing->thingClassId() == solaxEvcG2ThingClassId) {

        bool connectionWasCreated =
                m_tcpConnections.contains(thing) ||
                m_rtuConnections.contains(thing) ||
                m_evcG2TcpConnections.contains(thing);
        if (!connectionWasCreated) {
            qCDebug(dcSolax()) << "Aborting post setup, because setup did not complete.";
            return;
        }

        if (!m_pluginTimer) {
            qCDebug(dcSolax()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
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

                foreach(SolaxEvcG2ModbusTcpConnection *connection, m_evcG2TcpConnections) {
                    connection->update();
                }
            });
            m_pluginTimer->start();
        }
    }

    if (thing->thingClassId() == solaxX3InverterTCPThingClassId ||
            thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId).isEmpty()) {
            qCDebug(dcSolax()) << "Set up solax meter for" << thing;
            QString name = supportedThings().findById(solaxMeterThingClassId).displayName();
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(solaxMeterThingClassId, name, QString(), thing->id()));
        }
    }
}

void IntegrationPluginSolax::writePasswordToInverter(Thing *thing)
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
                writePasswordToInverter(thing);
            } else {
                qCWarning(dcSolax()) << "Successfully set unlock password";
            }
        });

    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        qCDebug(dcSolax()) << "Set unlock password";
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        ModbusRtuReply *reply = connection->setUnlockPassword(2014);
        connect(reply, &ModbusRtuReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &ModbusRtuReply::finished, thing, [this, thing, reply](){
            if (reply->error() != ModbusRtuReply::NoError) {
                qCWarning(dcSolax()) << "Error setting unlockpassword" << reply->error() << reply->errorString();
                writePasswordToInverter(thing);
            } else {
                qCWarning(dcSolax()) << "Successfully set unlock password";
            }
        });
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
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId){
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->reachable()) {
            qCWarning(dcSolax()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxX3InverterRTUSetExportLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3InverterRTUSetExportLimitActionExportLimitParamTypeId).toUInt();
            double ratedPower = thing->stateValue(solaxX3InverterRTUNominalPowerStateTypeId).toDouble();
            qCWarning(dcSolax()) << "Rated power is" << ratedPower;
            qCWarning(dcSolax()) << "Trying to set active power limit to" << powerLimit;
            quint16 target = powerLimit * (ratedPower/100); 
            ModbusRtuReply *reply = connection->setWriteExportLimit(target);
            connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
            connect(reply, &ModbusRtuReply::finished, info, [info, thing, reply, powerLimit, target](){
                if (reply->error() != ModbusRtuReply::NoError) {
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

        if (action.actionTypeId() == solaxBatteryEnableForcePowerActionTypeId) {
            bool state = action.paramValue(solaxBatteryEnableForcePowerActionEnableForcePowerParamTypeId).toBool();
            uint batteryTimeout = thing->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
            int powerToSet = thing->stateValue(solaxBatteryForcePowerStateTypeId).toInt();

            qCWarning(dcSolax()) << "Battery manual mode is enabled?" << state;
            if (state) {
                setBatteryPower(inverterThing, powerToSet, batteryTimeout);
            } else {
                disableRemoteControl(inverterThing);
            }

            // thing->setStateValue(solaxBatteryEnableForcePowerStateStateTypeId, state);
            thing->setStateValue(solaxBatteryEnableForcePowerStateTypeId, state);
        } else if (action.actionTypeId() == solaxBatteryForcePowerActionTypeId) {
            int batteryPower = action.paramValue(solaxBatteryForcePowerActionForcePowerParamTypeId).toInt();
            qCWarning(dcSolax()) << "Battery power should be set to" << batteryPower;
            thing->setStateValue(solaxBatteryForcePowerStateTypeId, batteryPower);

            uint timeout = thing->stateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId).toUInt();
            bool state = thing->stateValue(solaxBatteryEnableForcePowerStateStateTypeId).toBool();
            if (state)
            {
                setBatteryPower(inverterThing, batteryPower, timeout);
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
                setBatteryPower(inverterThing, batteryPower, timeout);
            }
        } else if (action.actionTypeId() == solaxBatteryMaxChargingCurrentActionTypeId) {
            double maxCurrent = action.paramValue(solaxBatteryMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toDouble();
            bool enableMaxCurrent = thing->stateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId).toBool();

            if (maxCurrent != thing->stateValue(solaxBatteryMaxChargingCurrentStateTypeId).toDouble()){
                qCWarning(dcSolax()) << "Battery max current changed to" << maxCurrent << "A";
                if (enableMaxCurrent && maxCurrent >= 0 && maxCurrent <= 30)
                {
                    thing->setStateValue(solaxBatteryMaxChargingCurrentStateTypeId, maxCurrent);
                    setMaxCurrent(inverterThing, maxCurrent);
                }  
            }          
        } else if (action.actionTypeId() == solaxBatteryEnableMaxChargingCurrentActionTypeId) {
            bool enableMaxCurrent = action.paramValue(solaxBatteryEnableMaxChargingCurrentActionEnableMaxChargingCurrentParamTypeId).toBool();
            double maxCurrent = thing->stateValue(solaxBatteryMaxChargingCurrentStateTypeId).toDouble();
            
            if (enableMaxCurrent != thing->stateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId).toBool()){
                qCWarning(dcSolax()) << "Enabling of battery max current changed to" << enableMaxCurrent;
                if (!enableMaxCurrent) {
                    setMaxCurrent(inverterThing, 30.0); //reset to default value 30A
                    thing->setStateValue(solaxBatteryMaxChargingCurrentStateTypeId, 30.0);
                } else if (enableMaxCurrent && maxCurrent >= 0 && maxCurrent <= 30)
                {
                    setMaxCurrent(inverterThing, maxCurrent);
                }
                thing->setStateValue(solaxBatteryEnableMaxChargingCurrentStateTypeId, enableMaxCurrent); 
            }                
        } else if (action.actionTypeId() == solaxBatteryMinBatteryLevelActionTypeId) {
            int minBatteryLevel = action.paramValue(solaxBatteryMinBatteryLevelActionMinBatteryLevelParamTypeId).toInt();
            qCWarning(dcSolax()) << "Min battery level set to" << minBatteryLevel;
            thing->setStateValue(solaxBatteryMinBatteryLevelStateTypeId, minBatteryLevel);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else if (thing->thingClassId() == solaxEvcG2ThingClassId) {
        SolaxEvcG2ModbusTcpConnection *connection = m_evcG2TcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }
        if (!connection->modbusTcpMaster()->connected()) {
            qCWarning(dcSolax()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxEvcG2PowerActionTypeId) {
            auto charging = action.paramValue(solaxEvcG2PowerActionPowerParamTypeId).toBool();
            setEvcG2Charging(connection, charging);
            thing->setStateValue(solaxEvcG2PowerStateTypeId, charging);
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == solaxEvcG2MaxChargingCurrentActionTypeId) {
            auto maxChargeCurrent =
                    action.paramValue(solaxEvcG2MaxChargingCurrentActionMaxChargingCurrentParamTypeId).toFloat();
            setEvcG2MaxChargingCurrent(connection, maxChargeCurrent);
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCDebug(dcSolax()) << "Unhandled action:" << info;
            info->finish(Thing::ThingErrorActionTypeNotFound);
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

    if (m_evcG2TcpConnections.contains(thing)) {
        SolaxEvcG2ModbusTcpConnection *connection = m_evcG2TcpConnections.take(thing);
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

void IntegrationPluginSolax::disableRemoteControl(Thing *thing)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "disableRemoteControl - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->stop();
        }

        QModbusReply *replyMode = connection->setModeType(0);
        connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
        connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode](){
            if (replyMode->error() != QModbusDevice::NoError) {
                qCWarning(dcSolax()) << "disableRemoteControl - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "disableRemoteControl - Mode set to 0";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
        Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
        if (!batteryThings.isEmpty()) {
            uint setTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
            batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, setTimeout);
        }
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "disableRemoteControl - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->stop();
        }

        ModbusRtuReply *replyMode = connection->setModeType(0);
        connect(replyMode, &ModbusRtuReply::finished, replyMode, &ModbusRtuReply::deleteLater);
        connect(replyMode, &ModbusRtuReply::finished, thing, [thing, replyMode](){
            if (replyMode->error() != ModbusRtuReply::NoError) {
                qCWarning(dcSolax()) << "disableRemoteControl - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "disableRemoteControl - Mode set to 0";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
        Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(solaxBatteryThingClassId);
        if (!batteryThings.isEmpty()) {
            uint setTimeout = batteryThings.first()->stateValue(solaxBatteryForcePowerTimeoutStateTypeId).toUInt();
            batteryThings.first()->setStateValue(solaxBatteryForcePowerTimeoutCountdownStateTypeId, setTimeout);
        }
    } else {
        qCWarning(dcSolax()) << "disableRemoteControl - Received wrong thing";
    }
    
}

void IntegrationPluginSolax::setBatteryPower(Thing *thing, qint32 powerToSet, quint16 batteryTimeout)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
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

        quint32 modeTypeValue = 4;
        QModbusReply *replyMode = connection->setModeType(modeTypeValue);
        connect(replyMode, &QModbusReply::finished, replyMode, &QModbusReply::deleteLater);
        connect(replyMode, &QModbusReply::finished, thing, [thing, replyMode, connection, powerToSet](){
            if (replyMode->error() != QModbusDevice::NoError) {
                qCWarning(dcSolax()) << "setBatteryPower - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "setBatteryPower - Mode set to 8, Type set to 1";
                //info->finish(Thing::ThingErrorNoError);
                QModbusReply *replyBatterPower = connection->setForceBatteryPower(-1*powerToSet);
                connect(replyBatterPower, &QModbusReply::finished, replyBatterPower, &QModbusReply::deleteLater);
                connect(replyBatterPower, &QModbusReply::finished, thing, [thing, replyBatterPower, powerToSet](){
                    if (replyBatterPower->error() != QModbusDevice::NoError) {
                        qCWarning(dcSolax()) << "setBatteryPower - Error setting battery power" << replyBatterPower->error() << replyBatterPower->errorString();
                        //info->finish(Thing::ThingErrorHardwareFailure);
                    } else {
                        qCWarning(dcSolax()) << "setBatteryPower - Set battery power to" << powerToSet;
                        //info->finish(Thing::ThingErrorNoError);
                    }
                });
            }
        });
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "setBatteryPower - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!m_batteryPowerTimer->isActive()) {
            m_batteryPowerTimer->setInterval(batteryTimeout*1000);
            m_batteryPowerTimer->start();
        }

        quint32 modeTypeValue = 4;
        ModbusRtuReply *replyMode = connection->setModeType(modeTypeValue);
        connect(replyMode, &ModbusRtuReply::finished, replyMode, &ModbusRtuReply::deleteLater);
        connect(replyMode, &ModbusRtuReply::finished, thing, [thing, replyMode, connection, powerToSet](){
            if (replyMode->error() != ModbusRtuReply::NoError) {
                qCWarning(dcSolax()) << "setBatteryPower - Error setting mode and type" << replyMode->error() << replyMode->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "setBatteryPower - Mode set to 8, Type set to 1";
                //info->finish(Thing::ThingErrorNoError);
                ModbusRtuReply *replyBatterPower = connection->setForceBatteryPower(-1*powerToSet);
                connect(replyBatterPower, &ModbusRtuReply::finished, replyBatterPower, &ModbusRtuReply::deleteLater);
                connect(replyBatterPower, &ModbusRtuReply::finished, thing, [thing, replyBatterPower, powerToSet](){
                    if (replyBatterPower->error() != ModbusRtuReply::NoError) {
                        qCWarning(dcSolax()) << "setBatteryPower - Error setting battery power" << replyBatterPower->error() << replyBatterPower->errorString();
                        //info->finish(Thing::ThingErrorHardwareFailure);
                    } else {
                        qCWarning(dcSolax()) << "setBatteryPower - Set battery power to" << powerToSet;
                        //info->finish(Thing::ThingErrorNoError);
                    }
                });
            }
        });
    } else {
        qCWarning(dcSolax()) << "setBatteryPower - Received incorrect thing";
    }
}

void IntegrationPluginSolax::setMaxCurrent(Thing *thing, double maxCurrent)
{
    if (thing->thingClassId() == solaxX3InverterTCPThingClassId) {
        SolaxModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "setBatteryPower - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        QModbusReply *reply = connection->setWriteChargeMaxCurrent(float(maxCurrent));
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, thing, [thing, reply](){
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcSolax()) << "setMaxChargingCurrent: Error during setting" << reply->error() << reply->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "setMaxChargingCurrent: set successfully ";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "setBatteryPower - Modbus connection not available";
            // info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        ModbusRtuReply *reply = connection->setWriteChargeMaxCurrent(float(maxCurrent));
        connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
        connect(reply, &ModbusRtuReply::finished, thing, [thing, reply](){
            if (reply->error() != ModbusRtuReply::NoError) {
                qCWarning(dcSolax()) << "setMaxChargingCurrent: Error during setting" << reply->error() << reply->errorString();
                //info->finish(Thing::ThingErrorHardwareFailure);
            } else {
                qCWarning(dcSolax()) << "setMaxChargingCurrent: set successfully ";
                //info->finish(Thing::ThingErrorNoError);
            }
        });
    } else {
        qCWarning(dcSolax()) << "setBatteryPower - Received incorrect thing";
    }
}

void IntegrationPluginSolax::configureEvcG2(SolaxEvcG2ModbusTcpConnection *connection)
{
    qCDebug(dcSolax()) << "Configuring Solax 2nd Gen wallbox...";

    const auto startChargeModeReply =
            connection->setStartChargeMode(SolaxEvcG2ModbusTcpConnection::StartChargeModePlugAndCharge);
    connect(startChargeModeReply, &QModbusReply::finished,
            startChargeModeReply, &QModbusReply::deleteLater);
    connect(startChargeModeReply, &QModbusReply::finished, this, [startChargeModeReply]()
    {
        if (startChargeModeReply->error() == QModbusDevice::NoError) {
            qCDebug(dcSolax()) << "Successfully set StartChargeMode to \"PlugAndCharge\"";
        } else {
            qCDebug(dcSolax()) << "Error while setting StartChargeMode:" << startChargeModeReply->error();
        }
    });

    QTimer::singleShot(1000, connection, [connection]() {
        const auto boostModeReply =
                connection->setBoostMode(SolaxEvcG2ModbusTcpConnection::BoostModeNormal);
        connect(boostModeReply, &QModbusReply::finished,
                boostModeReply, &QModbusReply::deleteLater);
        connect(boostModeReply, &QModbusReply::finished, boostModeReply, [boostModeReply]()
        {
            if (boostModeReply->error() == QModbusDevice::NoError) {
                qCDebug(dcSolax()) << "Successfully set Boost Mode to \"Normal\"";
            } else {
                qCDebug(dcSolax()) << "Error while setting Boost Mode:" << boostModeReply->error();
            }
        });
    });

    QTimer::singleShot(2000, connection, [connection]() {
        const auto evseSceneReply =
                connection->setEvseSceneRW(SolaxEvcG2ModbusTcpConnection::EvseSceneStandard);
        connect(evseSceneReply, &QModbusReply::finished,
                evseSceneReply, &QModbusReply::deleteLater);
        connect(evseSceneReply, &QModbusReply::finished, evseSceneReply, [evseSceneReply]()
        {
            if (evseSceneReply->error() == QModbusDevice::NoError) {
                qCDebug(dcSolax()) << "Successfully set EVSE Scene to \"Standard\"";
            } else {
                qCDebug(dcSolax()) << "Error while setting EVSE Scene:" << evseSceneReply->error();
            }
        });
    });

    QTimer::singleShot(3000, connection, [connection]() {
        const auto evseModeReply =
                connection->setEvseMode(SolaxEvcG2ModbusTcpConnection::EvseModeFast);
        connect(evseModeReply, &QModbusReply::finished,
                evseModeReply, &QModbusReply::deleteLater);
        connect(evseModeReply, &QModbusReply::finished, evseModeReply, [evseModeReply]()
        {
            if (evseModeReply->error() == QModbusDevice::NoError) {
                qCDebug(dcSolax()) << "Successfully set EVSE Mode to \"Fast\"";
            } else {
                qCDebug(dcSolax()) << "Error while setting EVSE Mode:" << evseModeReply->error();
            }
        });
    });
}

void IntegrationPluginSolax::setEvcG2Charging(SolaxEvcG2ModbusTcpConnection *connection,
                                              bool charging)
{
    qCDebug(dcSolax()) << "Current control command:" << connection->controlCommand();
    qCDebug(dcSolax()) << (charging ? "Start" : "Stop") << "charging";
    const auto command = charging ?
                SolaxEvcG2ModbusTcpConnection::ControlCommandStartCharge :
                SolaxEvcG2ModbusTcpConnection::ControlCommandStopCharge;
    const auto reply = connection->setControlCommand(command);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            qCDebug(dcSolax()) << "Successfully set charging"; // #TODO remove when testing finished
        } else {
            qCDebug(dcSolax())
                    << "Error while setting charging:"
                    << reply->error()
                    << reply->errorString();
        }
    });
}

void IntegrationPluginSolax::setEvcG2MaxChargingCurrent(SolaxEvcG2ModbusTcpConnection *connection,
                                                        float maxChargingCurrent)
{
    qCDebug(dcSolax()) << "Setting max. charging current to" << maxChargingCurrent << "A";
    const auto reply = connection->setMaxChargeCurrent(maxChargingCurrent);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            qCDebug(dcSolax()) << "Successfully set max. charging current"; // #TODO remove when testing finished
        } else {
            qCDebug(dcSolax())
                    << "Error while setting max. charging current:"
                    << reply->error()
                    << reply->errorString();
        }
    });
}

void IntegrationPluginSolax::writeErrorLog()
{
    qCWarning(dcSolax()) << "WriteErrorLog called";
    // Write to file /data/solax-counter.txt
    QFile errorCounterFile("/data/solax-counter.txt");

    int init = 0;
    int cont = 0;
    errorCounterFile.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
    while (!errorCounterFile.atEnd()) {
        QByteArray line = errorCounterFile.readLine();
        if (line.startsWith("Init")) {
            QList<QByteArray> list = line.split(' ');
            init = list[1].toInt();
        } else {
            QList<QByteArray> list = line.split(' ');
            cont = list[1].toInt();
        }
    }

    if (m_energyCheck == 500) {
        init += 1;
    } else {
        cont += 1;
    }
    errorCounterFile.write("Init: " + QByteArray::number(init) + QByteArray("\nCont: ") + QByteArray::number(cont));
    errorCounterFile.close();
}
