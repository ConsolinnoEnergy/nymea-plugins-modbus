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

        /*
        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        DiscoveryTcp *discovery = new DiscoveryTcp(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &DiscoveryTcp::discoveryFinished, info, [=](){
            foreach (const DiscoveryTcp::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxX3InverterTCPThingClassId, result.manufacturerName + " " + result.productName, QString("rated power: %1").arg(result.powerRating) + " - " + result.networkDeviceInfo.address().toString());
                qCInfo(dcSolax()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxX3InverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolax()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(solaxX3InverterTCPThingMacAddressParamTypeId, result.networkDeviceInfo.address().toString());
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

        */

        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){

            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

                qCDebug(dcSolax()) << "Found" << networkDeviceInfo;

                QString title;
                if (networkDeviceInfo.hostName().isEmpty()) {
                    title = networkDeviceInfo.address().toString();
                } else {
                    title = networkDeviceInfo.hostName() + " (" + networkDeviceInfo.address().toString() + ")";
                }

                QString description;
                if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                    description = networkDeviceInfo.macAddress();
                } else {
                    description = networkDeviceInfo.macAddress() + " (" + networkDeviceInfo.macAddressManufacturer() + ")";
                }

                ThingDescriptor descriptor(solaxX3InverterTCPThingClassId, title, description);
                ParamList params;
                params << Param(solaxX3InverterTCPThingIpAddressParamTypeId, networkDeviceInfo.address().toString());
                params << Param(solaxX3InverterTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                params << Param(solaxX3InverterTCPThingPortParamTypeId, 502);
                params << Param(solaxX3InverterTCPThingModbusIdParamTypeId, 1);
                descriptor.setParams(params);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxX3InverterTCPThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcSolax()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

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
                ThingDescriptor descriptor(info->thingClassId(), "Solax Inverter", QString("Modbus ID: %1").arg(result.modbusId));

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
            delete m_tcpConnections.take(thing);
        } else {
            qCDebug(dcSolax()) << "Setting up a new device:" << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);


        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
        MacAddress macAddress = MacAddress(thing->paramValue(solaxX3InverterTCPThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolax()) << "Failed to set up Solax inverter because the MAC address is not valid:" << thing->paramValue(solaxX3InverterTCPThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
        });

        uint port = thing->paramValue(solaxX3InverterTCPThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxX3InverterTCPThingModbusIdParamTypeId).toUInt();

        SolaxModbusTcpConnection *connection = new SolaxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        connect(info, &ThingSetupInfo::aborted, connection, &SolaxModbusTcpConnection::deleteLater);

        connect(connection, &SolaxModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
            qCDebug(dcSolax()) << "Reachable state changed" << reachable;
            if (reachable) {
                connection->initialize();
            } else {
                thing->setStateValue(solaxX3InverterTCPConnectedStateTypeId, false);
            }
        });

        connect(connection, &SolaxModbusTcpConnection::initializationFinished, info, [this, thing, connection, monitor, info](bool success){
            if (success) {
                qCDebug(dcSolax()) << "Solax inverter initialized.";
                m_tcpConnections.insert(thing, connection);
                m_monitors.insert(thing, monitor);
                MeterStates meterStates{};
                m_meterstates.insert(thing, meterStates);
                BatteryStates batteryStates{};
                m_batterystates.insert(thing, batteryStates);
                thing->setStateValue(solaxX3InverterTCPConnectedStateTypeId, true);
                connection->update();
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                connection->deleteLater();
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("Could not initialize the communication with the inverter."));
            }
        });


        // Handle property changed signals for inverter
        connect(connection, &SolaxModbusTcpConnection::inverterPowerChanged, thing, [thing](qint16 inverterPower){
            qCDebug(dcSolax()) << "Inverter power changed" << inverterPower << "W";
            thing->setStateValue(solaxX3InverterTCPCurrentPowerStateTypeId, -inverterPower);
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

        connect(connection, &SolaxModbusTcpConnection::activePowerLimitChanged, thing, [thing](quint16 activePowerLimit){
            qCDebug(dcSolax()) << "Inverter active power limit changed" << activePowerLimit << "%";
            thing->setStateValue(solaxX3InverterTCPActivePowerLimitStateTypeId, activePowerLimit);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolax()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });

        connect(connection, &SolaxModbusTcpConnection::inverterTypeChanged, thing, [thing](quint16 inverterRatedPower){
            qCDebug(dcSolax()) << "Inverter rated power changed" << inverterRatedPower << "W";
            thing->setStateValue(solaxX3InverterTCPRatedPowerStateTypeId, inverterRatedPower);
        });


        // Meter
        connect(connection, &SolaxModbusTcpConnection::meter1CommunicationSateChanged, thing, [this, thing](quint16 commStatus){
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
                ThingDescriptor descriptor(solaxBatteryThingClassId, "Solax battery", QString(), thing->id());
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


        connection->connectDevice();

        return;
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
        connect(connection, &SolaxModbusRtuConnection::inverterPowerChanged, thing, [thing](qint16 inverterPower){
            qCDebug(dcSolax()) << "Inverter power changed" << inverterPower << "W";
            thing->setStateValue(solaxX3InverterRTUCurrentPowerStateTypeId, -inverterPower);
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

        connect(connection, &SolaxModbusRtuConnection::activePowerLimitChanged, thing, [thing](quint16 activePowerLimit){
            qCDebug(dcSolax()) << "Inverter active power limit changed" << activePowerLimit << "%";
            thing->setStateValue(solaxX3InverterRTUActivePowerLimitStateTypeId, activePowerLimit);
        });

        connect(connection, &SolaxModbusRtuConnection::inverterFaultBitsChanged, thing, [this, thing](quint32 inverterFaultBits){
            qCDebug(dcSolax()) << "Inverter fault bits recieved" << inverterFaultBits;
            setErrorMessage(thing, inverterFaultBits);
        });

        connect(connection, &SolaxModbusRtuConnection::inverterTypeChanged, thing, [thing](quint16 inverterRatedPower){
            qCDebug(dcSolax()) << "Inverter rated power changed" << inverterRatedPower << "W";
            thing->setStateValue(solaxX3InverterRTURatedPowerStateTypeId, inverterRatedPower);
        });


        // Meter
        connect(connection, &SolaxModbusRtuConnection::meter1CommunicationSateChanged, thing, [this, thing](quint16 commStatus){
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
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcSolax()) << "Set up Solax battery for" << thing;
                ThingDescriptor descriptor(solaxBatteryThingClassId, "Solax battery", QString(), thing->id());
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
            }

            // Set capacity from parent parameter.
            if (parentThing->thingClassId() == solaxX3InverterTCPThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3InverterTCPThingBatteryCapacityParamTypeId).toUInt());
            } else if (parentThing->thingClassId() == solaxX3InverterRTUThingClassId) {
                thing->setStateValue(solaxBatteryCapacityStateTypeId, parentThing->paramValue(solaxX3InverterRTUThingBatteryCapacityParamTypeId).toUInt());
            }
        }
        return;
    }
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
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(solaxMeterThingClassId).isEmpty()) {
            qCDebug(dcSolax()) << "Set up solax meter for" << thing;
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

        if (action.actionTypeId() == solaxX3InverterTCPActivePowerLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3InverterTCPActivePowerLimitActionActivePowerLimitParamTypeId).toUInt();
            qCDebug(dcSolax()) << "Trying to set active power limit to" << powerLimit;
            QModbusReply *reply = connection->setActivePowerLimit(powerLimit);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, thing, reply, powerLimit](){
                if (reply->error() != QModbusDevice::NoError) {
                    qCWarning(dcSolax()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCDebug(dcSolax()) << "Active power limit set to" << powerLimit;
                    thing->setStateValue(solaxX3InverterTCPActivePowerLimitStateTypeId, powerLimit);
                    info->finish(Thing::ThingErrorNoError);
                }
            });
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled action: %1").arg(actionType.name()).toUtf8());
        }
    } else if (thing->thingClassId() == solaxX3InverterRTUThingClassId) {
        SolaxModbusRtuConnection *connection = m_rtuConnections.value(thing);
        if (!connection) {
            qCWarning(dcSolax()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!connection->modbusRtuMaster()->connected()) {
            qCWarning(dcSolax()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (action.actionTypeId() == solaxX3InverterRTUActivePowerLimitActionTypeId) {
            quint16 powerLimit = action.paramValue(solaxX3InverterRTUActivePowerLimitActionActivePowerLimitParamTypeId).toUInt();
            qCDebug(dcSolax()) << "Trying to set active power limit to" << powerLimit;
            ModbusRtuReply *reply = connection->setActivePowerLimit(powerLimit);
            connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
            connect(reply, &ModbusRtuReply::finished, info, [info, thing, reply, powerLimit](){
                if (reply->error() != ModbusRtuReply::NoError) {
                    qCWarning(dcSolax()) << "Error setting active power limit" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    qCDebug(dcSolax()) << "Active power limit set to" << powerLimit;
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
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
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
