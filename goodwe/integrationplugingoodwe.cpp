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

#include "integrationplugingoodwe.h"
#include "plugininfo.h"
#include "goodwediscovery.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

#include <QtMath>

IntegrationPluginGoodwe::IntegrationPluginGoodwe()
{

}

void IntegrationPluginGoodwe::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcGoodwe()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(goodweInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcGoodwe()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(goodweInverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    // Support for multiple batteries.
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(goodweBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginGoodwe::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == goodweInverterRTUThingClassId) {
        GoodweDiscovery *discovery = new GoodweDiscovery(hardwareManager()->modbusRtuResource(), info);

        connect(discovery, &GoodweDiscovery::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with appropriate settings found. Please set up a modbus RTU master."));
                return;
            }

            qCInfo(dcGoodwe()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const GoodweDiscovery::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(goodweInverterRTUThingClassId, "Goodwe Inverter", QString("Slave ID: %1").arg(result.slaveId));

                ParamList params{
                    {goodweInverterRTUThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {goodweInverterRTUThingSlaveAddressParamTypeId, result.slaveId}
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

        /*
        qCDebug(dcGoodwe()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(goodweInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress < 1) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcGoodwe()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "GoodWe Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(goodweInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(goodweInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
        */
    }
}

void IntegrationPluginGoodwe::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcGoodwe()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == goodweInverterRTUThingClassId) {

        uint address = thing->paramValue(goodweInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 1) {
            qCWarning(dcGoodwe()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(goodweInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcGoodwe()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcGoodwe()) << "Already have a GoodWe connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_meterstates.contains(thing))
            m_meterstates.remove(thing);

        if (m_batterystates.contains(thing))
            m_batterystates.remove(thing);

        GoodweModbusRtuConnection *connection = new GoodweModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &GoodweModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(goodweInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterstates.find(thing)->modbusReachable = reachable;
                if (reachable && m_meterstates.value(thing).meterCommStatus) {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                m_batterystates.find(thing)->modbusReachable = reachable;
                if (reachable && m_batterystates.value(thing).mode) {
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, false);
                }
            }

            if (reachable) {
                qCDebug(dcGoodwe()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcGoodwe()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });


        // Handle property changed signals
        connect(connection, &GoodweModbusRtuConnection::pv1PowerChanged, this, [this, thing](quint32 pv1Power){
            qCDebug(dcGoodwe()) << "Inverter PV1 power changed" << pv1Power << "W";
            thing->setStateValue(goodweInverterRTUPv1PowerStateTypeId, pv1Power);
            double pv2Power = thing->stateValue(goodweInverterRTUPv2PowerStateTypeId).toDouble();
            double pv3Power = thing->stateValue(goodweInverterRTUPv3PowerStateTypeId).toDouble();
            double pv4Power = thing->stateValue(goodweInverterRTUPv4PowerStateTypeId).toDouble();
            double totalPvPower = (double)pv1Power + pv2Power + pv3Power + pv4Power;
            thing->setStateValue(goodweInverterRTUCurrentPowerStateTypeId, -totalPvPower);
        });

        connect(connection, &GoodweModbusRtuConnection::pv1VoltageChanged, this, [this, thing](quint16 pv1Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV1 voltage changed" << pv1Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv1VoltageStateTypeId, pv1Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv1CurrentChanged, this, [this, thing](quint16 pv1Current){
            qCDebug(dcGoodwe()) << "Inverter PV1 current changed" << pv1Current << "A";
            thing->setStateValue(goodweInverterRTUPv1CurrentStateTypeId, pv1Current);
        });

        connect(connection, &GoodweModbusRtuConnection::pv2PowerChanged, this, [this, thing](quint32 pv2Power){
            qCDebug(dcGoodwe()) << "Inverter PV2 power changed" << pv2Power << "W";
            thing->setStateValue(goodweInverterRTUPv2PowerStateTypeId, pv2Power);
            double pv1Power = thing->stateValue(goodweInverterRTUPv1PowerStateTypeId).toDouble();
            double pv3Power = thing->stateValue(goodweInverterRTUPv3PowerStateTypeId).toDouble();
            double pv4Power = thing->stateValue(goodweInverterRTUPv4PowerStateTypeId).toDouble();
            double totalPvPower = pv1Power + (double)pv2Power + pv3Power + pv4Power;
            thing->setStateValue(goodweInverterRTUCurrentPowerStateTypeId, -totalPvPower);
        });

        connect(connection, &GoodweModbusRtuConnection::pv2VoltageChanged, this, [this, thing](quint16 pv2Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV2 voltage changed" << pv2Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv2VoltageStateTypeId, pv2Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv2CurrentChanged, this, [this, thing](quint16 pv2Current){
            qCDebug(dcGoodwe()) << "Inverter PV2 current changed" << pv2Current << "A";
            thing->setStateValue(goodweInverterRTUPv2CurrentStateTypeId, pv2Current);
        });

        connect(connection, &GoodweModbusRtuConnection::pv3PowerChanged, this, [this, thing](quint32 pv3Power){
            qCDebug(dcGoodwe()) << "Inverter PV3 power changed" << pv3Power << "W";
            thing->setStateValue(goodweInverterRTUPv3PowerStateTypeId, pv3Power);
            double pv1Power = thing->stateValue(goodweInverterRTUPv1PowerStateTypeId).toDouble();
            double pv2Power = thing->stateValue(goodweInverterRTUPv2PowerStateTypeId).toDouble();
            double pv4Power = thing->stateValue(goodweInverterRTUPv4PowerStateTypeId).toDouble();
            double totalPvPower = pv1Power + pv2Power + (double)pv3Power + pv4Power;
            thing->setStateValue(goodweInverterRTUCurrentPowerStateTypeId, -totalPvPower);
        });

        connect(connection, &GoodweModbusRtuConnection::pv3VoltageChanged, this, [this, thing](quint16 pv3Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV3 voltage changed" << pv3Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv3VoltageStateTypeId, pv3Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv3CurrentChanged, this, [this, thing](quint16 pv3Current){
            qCDebug(dcGoodwe()) << "Inverter PV3 current changed" << pv3Current << "A";
            thing->setStateValue(goodweInverterRTUPv3CurrentStateTypeId, pv3Current);
        });

        connect(connection, &GoodweModbusRtuConnection::pv4PowerChanged, this, [this, thing](quint32 pv4Power){
            qCDebug(dcGoodwe()) << "Inverter PV4 power changed" << pv4Power << "W";
            thing->setStateValue(goodweInverterRTUPv4PowerStateTypeId, pv4Power);
            double pv1Power = thing->stateValue(goodweInverterRTUPv1PowerStateTypeId).toDouble();
            double pv2Power = thing->stateValue(goodweInverterRTUPv2PowerStateTypeId).toDouble();
            double pv3Power = thing->stateValue(goodweInverterRTUPv3PowerStateTypeId).toDouble();
            double totalPvPower = pv1Power + pv2Power + pv3Power + (double)pv4Power;
            thing->setStateValue(goodweInverterRTUCurrentPowerStateTypeId, -totalPvPower);
        });

        connect(connection, &GoodweModbusRtuConnection::pv4VoltageChanged, this, [this, thing](quint16 pv4Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV4 voltage changed" << pv4Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv4VoltageStateTypeId, pv4Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv4CurrentChanged, this, [this, thing](quint16 pv4Current){
            qCDebug(dcGoodwe()) << "Inverter PV4 current changed" << pv4Current << "A";
            thing->setStateValue(goodweInverterRTUPv4CurrentStateTypeId, pv4Current);
        });

        connect(connection, &GoodweModbusRtuConnection::pvEtotalChanged, this, [this, thing](float totalEnergyProduced){
            qCDebug(dcGoodwe()) << "Inverter total energy produced changed" << totalEnergyProduced << "kWh";
            thing->setStateValue(goodweInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        connect(connection, &GoodweModbusRtuConnection::workModeChanged, this, [this, thing](GoodweModbusRtuConnection::WorkMode workMode){
            qCDebug(dcGoodwe()) << "Inverter work mode recieved" << workMode;
            setWorkMode(thing, workMode);
        });


        // Meter
        connect(connection, &GoodweModbusRtuConnection::meterCommStatusChanged, thing, [this, thing](quint16 commStatus){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter comm status changed" << commStatus;
                bool commStatusBool{commStatus};
                m_meterstates.find(thing)->meterCommStatus = commStatusBool;
                if (commStatusBool && m_meterstates.value(thing).modbusReachable) {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, false);
                }
            }
        });

        // I don't really know how this works. Better leave it out. Register reads 0 on inverter.
        /*
        connect(connection, &GoodweModbusRtuConnection::meterConnectStatusChanged, thing, [this, thing](quint16 connectStatus){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Phases wiring indicator (MeterConnectStatus) changed" << connectStatus;
                // Probably need to parse this, otherwise it will be confusing. I think the value 273 means everything is ok.
                meterThings.first()->setStateValue(goodweMeterPhasesWiringIndicatorStateTypeId, connectStatus);
            }
        });
        */

        connect(connection, &GoodweModbusRtuConnection::meterTotalActivePowerChanged, thing, [this, thing](qint32 currentPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter power (meterTotalActivePower) changed" << currentPower << "W";
                // Check if sign is correct for power to grid and power from grid.
                meterThings.first()->setStateValue(goodweMeterCurrentPowerStateTypeId, currentPower);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::eTotalSellChanged, thing, [this, thing](float totalEnergyProduced){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter total energy produced (eTotalSell) changed" << totalEnergyProduced << "kWh";
                meterThings.first()->setStateValue(goodweMeterTotalEnergyProducedStateTypeId, totalEnergyProduced);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::eTotalBuyChanged, thing, [this, thing](float totalEnergyConsumed){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter total energy consumed (eTotalBuy) changed" << totalEnergyConsumed << "kWh";
                meterThings.first()->setStateValue(goodweMeterTotalEnergyConsumedStateTypeId, totalEnergyConsumed);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::iGridRChanged, thing, [this, thing](float currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current phase A (iGridR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(goodweMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::iGridSChanged, thing, [this, thing](float currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current phase B (iGridS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(goodweMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::iGridTChanged, thing, [this, thing](float currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current phase C (iGridT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(goodweMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerRChanged, thing, [this, thing](qint32 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase A (meterActivePowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseAStateTypeId, -currentPowerPhaseA);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerSChanged, thing, [this, thing](qint32 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase B (meterActivePowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseBStateTypeId, -currentPowerPhaseB);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerTChanged, thing, [this, thing](qint32 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase C (meterActivePowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseCStateTypeId, -currentPowerPhaseC);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::vGridRChanged, thing, [this, thing](float voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter voltage phase A (vGridR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(goodweMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::vGridSChanged, thing, [this, thing](float voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter voltage phase B (vGridS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(goodweMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::vGridTChanged, thing, [this, thing](float voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter voltage phase C (vGridT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(goodweMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterFrequenceChanged, thing, [this, thing](float frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter frequency changed" << frequency << "Hz";
                meterThings.first()->setStateValue(goodweMeterFrequencyStateTypeId, frequency);
            }
        });



        // Battery
        connect(connection, &GoodweModbusRtuConnection::batteryModeChanged, thing, [this, thing](GoodweModbusRtuConnection::BatteryStatus mode){

            qCDebug(dcGoodwe()) << "Battery mode changed" << mode;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            // mode = 0 means no battery
            if (mode) {

                if (batteryThings.isEmpty()) {
                    // No battery exists yet. Create it.
                    qCDebug(dcGoodwe()) << "Set up goodwe energy storage for" << thing;
                    ThingDescriptor descriptor(goodweBatteryThingClassId, "Goodwe battery", QString(), thing->id());
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);
                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // mode before value will be set.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
                }

                if (!batteryThings.isEmpty()) {
                    m_batterystates.find(thing)->mode = mode;
                    if (m_batterystates.value(thing).modbusReachable) {
                        batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, true);
                    }
                    switch (mode) {
                        case GoodweModbusRtuConnection::BatteryStatusStandby:
                            batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "idle");
                            break;
                        case GoodweModbusRtuConnection::BatteryStatusDischarging:
                            batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "discharging");
                            break;
                        case GoodweModbusRtuConnection::BatteryStatusCharging:
                            batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "charging");
                            break;
                        default:
                            break;
                    }
                }

            } else if (!batteryThings.isEmpty()) {
                m_batterystates.find(thing)->mode = mode;
                batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, false);

                // Can add code here to remove battery from things.
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batterySohChanged, thing, [this, thing](quint16 batSoh){
            qCDebug(dcGoodwe()) << "Battery state of health changed" << batSoh << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, batSoh);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batteryPowerChanged, thing, [this, thing](qint16 batPower){
            qCDebug(dcGoodwe()) << "Battery power changed" << batPower << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                // Modbus register batPower is negative when charging. However, Nymea json value is positive when charging.
                batteryThings.first()->setStateValue(goodweBatteryCurrentPowerStateTypeId, -batPower);
                /*
                if (batPower > 0) {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "discharging");
                } else if (batPower < 0) {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "idle");
                }
                */
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batterySocChanged, thing, [this, thing](quint16 socBat){
            qCDebug(dcGoodwe()) << "Battery state of charge changed" << socBat << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryBatteryLevelStateTypeId, socBat);
                batteryThings.first()->setStateValue(goodweBatteryBatteryCriticalStateTypeId, socBat < 10);
            }
        });


        // FIXME: make async and check if this is really a Goodwe
        m_rtuConnections.insert(thing, connection);
        MeterStates meterStates{};
        m_meterstates.insert(thing, meterStates);
        BatteryStates batteryStates{};
        m_batterystates.insert(thing, batteryStates);
        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == goodweMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(goodweMeterConnectedStateTypeId, parentThing->stateValue(goodweInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }

    if (thing->thingClassId() == goodweBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);

        /*
        // Set battery capacity from settings on restart.
        thing->setStateValue(goodweBatteryCapacityStateTypeId, thing->setting(goodweBatterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == goodweBatterySettingsCapacityParamTypeId) {
                qCDebug(dcGoodwe()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(goodweBatteryCapacityStateTypeId, value.toInt());
            }
        });
        */

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(goodweBatteryConnectedStateTypeId, parentThing->stateValue(goodweInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }
}

void IntegrationPluginGoodwe::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == goodweInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcGoodwe()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(GoodweModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId).isEmpty()) {
            qCDebug(dcGoodwe()) << "Set up GoodWe meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(goodweMeterThingClassId, "GoodWe Power Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginGoodwe::thingRemoved(Thing *thing)
{
    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (m_meterstates.contains(thing))
        m_meterstates.remove(thing);

    if (m_batterystates.contains(thing))
        m_batterystates.remove(thing);

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginGoodwe::setWorkMode(Thing *thing, GoodweModbusRtuConnection::WorkMode state)
{
    switch (state) {
        case GoodweModbusRtuConnection::WorkModeWait:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Wait");
            break;
        case GoodweModbusRtuConnection::WorkModeOnGrid:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "On grid");
            break;
        case GoodweModbusRtuConnection::WorkModeOffGrid:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Off grid");
            break;
        case GoodweModbusRtuConnection::WorkModeFault:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Fault");
            break;
        case GoodweModbusRtuConnection::WorkModeInverterUpgrade:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Inverter upgradeing");
            break;
        case GoodweModbusRtuConnection::WorkModeSelfCheck:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Self check");
            break;
    }
}
