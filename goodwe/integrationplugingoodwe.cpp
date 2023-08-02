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
                if (reachable && m_batterystates.value(thing).mode && m_batterystates.value(thing).bmsCommStatus) {
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

        connect(connection, &GoodweModbusRtuConnection::pv1VoltageChanged, this, [this, thing](float pv1Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV1 voltage changed" << pv1Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv1VoltageStateTypeId, pv1Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv1CurrentChanged, this, [this, thing](float pv1Current){
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

        connect(connection, &GoodweModbusRtuConnection::pv2VoltageChanged, this, [this, thing](float pv2Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV2 voltage changed" << pv2Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv2VoltageStateTypeId, pv2Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv2CurrentChanged, this, [this, thing](float pv2Current){
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

        connect(connection, &GoodweModbusRtuConnection::pv3VoltageChanged, this, [this, thing](float pv3Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV3 voltage changed" << pv3Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv3VoltageStateTypeId, pv3Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv3CurrentChanged, this, [this, thing](float pv3Current){
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

        connect(connection, &GoodweModbusRtuConnection::pv4VoltageChanged, this, [this, thing](float pv4Voltage){
            qCDebug(dcGoodwe()) << "Inverter PV4 voltage changed" << pv4Voltage << "V";
            thing->setStateValue(goodweInverterRTUPv4VoltageStateTypeId, pv4Voltage);
        });

        connect(connection, &GoodweModbusRtuConnection::pv4CurrentChanged, this, [this, thing](float pv4Current){
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

        connect(connection, &GoodweModbusRtuConnection::errorMessageChanged, this, [this, thing](quint32 errorBits){
            qCDebug(dcGoodwe()) << "Inverter error bits changed" << errorBits;
            setInverterErrorMessage(thing, errorBits);
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

        connect(connection, &GoodweModbusRtuConnection::meterPowerFactorChanged, thing, [this, thing](float powerFactor){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter power factor changed" << powerFactor;
                meterThings.first()->setStateValue(goodweMeterPowerFactorStateTypeId, powerFactor);
            }
        });



        // Battery
        connect(connection, &GoodweModbusRtuConnection::bmsCommStatusChanged, thing, [this, thing](quint16 bmsCommStatus){
            // Debug output even if there is no battery thing, since this signal creates it.
            qCDebug(dcGoodwe()) << "Battery BMS comm status changed" << bmsCommStatus;
            bool bmsCommStatusBool{bmsCommStatus};
            m_batterystates.find(thing)->bmsCommStatus = bmsCommStatusBool;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                if (bmsCommStatusBool && m_batterystates.value(thing).modbusReachable && m_batterystates.value(thing).mode) {
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, true);
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, false);
                }
            } else if (bmsCommStatusBool){
                // Battery detected. No battery exists yet. Create it.
                qCDebug(dcGoodwe()) << "Set up goodwe energy storage for" << thing;
                ThingDescriptor descriptor(goodweBatteryThingClassId, "Goodwe battery", QString(), thing->id());
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batteryModeChanged, thing, [this, thing](GoodweModbusRtuConnection::BatteryStatus mode){            
            m_batterystates.find(thing)->mode = mode;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery mode changed" << mode;
                if (mode && m_batterystates.value(thing).modbusReachable && m_batterystates.value(thing).bmsCommStatus) {
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, true);
                } else {
                    // mode = 0 means no battery
                    batteryThings.first()->setStateValue(goodweBatteryConnectedStateTypeId, false);
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
        });

        connect(connection, &GoodweModbusRtuConnection::batterySohChanged, thing, [this, thing](quint16 batSoh){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery state of health changed" << batSoh << "%";
                batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, batSoh);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batteryPowerChanged, thing, [this, thing](qint16 batPower){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery power changed" << batPower << "W";
                // Modbus register batPower is negative when charging. However, Nymea json value is positive when charging.
                batteryThings.first()->setStateValue(goodweBatteryCurrentPowerStateTypeId, -batPower);
                /*
                // There is a register for charging state. Use that instead.
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
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery state of charge changed" << socBat << "%";
                batteryThings.first()->setStateValue(goodweBatteryBatteryLevelStateTypeId, socBat);
                batteryThings.first()->setStateValue(goodweBatteryBatteryCriticalStateTypeId, socBat < 10);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::batteryTempChanged, thing, [this, thing](float batteryTemp){
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery temperature changed" << batteryTemp << "°C";
                batteryThings.first()->setStateValue(goodweBatteryTemperatureStateTypeId, batteryTemp);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::bmsWarningCodeLChanged, thing, [this, thing](quint16 bmsWarningCodeL){
            m_batterystates.find(thing)->bmsWarningCodeL = bmsWarningCodeL;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery BMS warning code L changed" << bmsWarningCodeL;
                setBatteryWarningMessage(thing);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::bmsWarningCodeHChanged, thing, [this, thing](quint16 bmsWarningCodeH){
            m_batterystates.find(thing)->bmsWarningCodeH = bmsWarningCodeH;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery BMS warning code H changed" << bmsWarningCodeH;
                setBatteryWarningMessage(thing);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::bmsErrorCodeLChanged, thing, [this, thing](quint16 bmsErrorCodeL){
            m_batterystates.find(thing)->bmsErrorCodeL = bmsErrorCodeL;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery BMS error code L changed" << bmsErrorCodeL;
                setBatteryErrorMessage(thing);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::bmsErrorCodeHChanged, thing, [this, thing](quint16 bmsErrorCodeH){
            m_batterystates.find(thing)->bmsErrorCodeH = bmsErrorCodeH;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Battery BMS error code H changed" << bmsErrorCodeH;
                setBatteryErrorMessage(thing);
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
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(goodweBatteryConnectedStateTypeId, parentThing->stateValue(goodweInverterRTUConnectedStateTypeId).toBool());
            thing->setStateValue(goodweBatteryCapacityStateTypeId, parentThing->paramValue(goodweInverterRTUThingBatteryCapacityParamTypeId).toUInt());
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

void IntegrationPluginGoodwe::setInverterErrorMessage(Thing *thing, quint32 errorBits)
{
    QString errorMessage{""};
    if (errorBits == 0) {
        errorMessage.append("No error, everything ok.");
    } else {
        errorMessage.append("The following error bits are active: ");
        quint32 testBit{0b01};
        for (int var = 0; var < 32; ++var) {
            if (errorBits & (testBit << var)) {
                errorMessage.append(QStringLiteral("bit %1, ").arg(var));
                if (var == 0) {
                    errorMessage.append(" The GFCI detecting circuit is abnormal, ");
                } else if (var == 1) {
                    errorMessage.append(" The output current sensor is abnormal, ");
                } else if (var == 3) {
                    errorMessage.append(" Different DCI value on Master and Slave, ");
                } else if (var == 4) {
                    errorMessage.append(" Different GFCI values on Master &Slave, ");
                } else if (var == 6) {
                    errorMessage.append(" GFCI check failure 3 times, ");
                } else if (var == 7) {
                    errorMessage.append(" Relay check failure 3 times, ");
                } else if (var == 8) {
                    errorMessage.append(" AC HCT check failure 3 times, ");
                } else if (var == 9) {
                    errorMessage.append(" Utility is unavailable, ");
                } else if (var == 10) {
                    errorMessage.append(" Ground current is too high, ");
                } else if (var == 11) {
                    errorMessage.append(" Dc bus is too high, ");
                } else if (var == 12) {
                    errorMessage.append(" The fan in case failure, ");
                } else if (var == 13) {
                    errorMessage.append(" Temperature is too high, ");
                } else if (var == 14) {
                    errorMessage.append(" Utility Phase Failure, ");
                } else if (var == 15) {
                    errorMessage.append(" Pv input voltage is over the tolerable maximum value, ");
                } else if (var == 16) {
                    errorMessage.append(" The external fan failure, ");
                } else if (var == 17) {
                    errorMessage.append(" Grid voltage out of tolerable range, ");
                } else if (var == 18) {
                    errorMessage.append(" Isolation resistance of PV-plant too low, ");
                } else if (var == 19) {
                    errorMessage.append(" The DC injection to grid is too high, ");
                } else if (var == 20) {
                    errorMessage.append(" Back-Up Over Load, ");
                } else if (var == 22) {
                    errorMessage.append(" Different value between Master and Slave for grid frequency, ");
                } else if (var == 23) {
                    errorMessage.append(" Different value between Master and Slave for grid voltage, ");
                } else if (var == 25) {
                    errorMessage.append(" Relay check is failure, ");
                } else if (var == 27) {
                    errorMessage.append(" Phase angle out of range (110°~140°), ");
                } else if (var == 28) {
                    errorMessage.append(" Communication between ARM and DSP is failure, ");
                } else if (var == 29) {
                    errorMessage.append(" The grid frequency is out of tolerable range, ");
                } else if (var == 30) {
                    errorMessage.append(" EEPROM cannot be read or written, ");
                } else if (var == 31) {
                    errorMessage.append(" Communication between microcontrollers is failure, ");
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
    qCDebug(dcGoodwe()) << errorMessage;
    thing->setStateValue(goodweInverterRTUErrorMessageStateTypeId, errorMessage);
}

void IntegrationPluginGoodwe::setBatteryWarningMessage(Thing *thing)
{
    Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
    if (!batteryThings.isEmpty()) {
        quint16 warningBitsL = m_batterystates.value(thing).bmsWarningCodeL;
        quint16 warningBitsH = m_batterystates.value(thing).bmsWarningCodeH;
        QString warningMessage{""};
        if (warningBitsL == 0 && warningBitsH == 0) {
            warningMessage.append("No warning, everything ok.");
        } else {
            warningMessage.append("The following warning bits are active: ");
            quint16 testBit{0b01};
            for (int var = 0; var < 16; ++var) {
                if (warningBitsL & (testBit << var)) {
                    warningMessage.append(QStringLiteral("bit %1").arg(var));
                    if (var == 3) {
                        warningMessage.append(" Cell Low temperature1, ");
                    } else if (var == 4) {
                        warningMessage.append(" Charge over-current1, ");
                    } else if (var == 5) {
                        warningMessage.append(" Discharge over-, ");
                    } else if (var == 6) {
                        warningMessage.append(" communication, ");
                    } else if (var == 7) {
                        warningMessage.append(" System Reboot, ");
                    } else if (var == 11) {
                        warningMessage.append(" System High temperature, ");
                    } else {
                        warningMessage.append(", ");
                    }
                }
            }
            for (int var = 0; var < 16; ++var) {
                if (warningBitsH & (testBit << var)) {
                    warningMessage.append(QStringLiteral("bit %1, ").arg(var + 16));
                }
            }
            int stringLength = warningMessage.length();
            if (stringLength > 1) { // stringLength should be > 1, but just in case.
                warningMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
            }
        }
        qCDebug(dcGoodwe()) << warningMessage;
        batteryThings.first()->setStateValue(goodweBatteryWarningMessageStateTypeId, warningMessage);
    }
}

void IntegrationPluginGoodwe::setBatteryErrorMessage(Thing *thing)
{
    Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
    if (!batteryThings.isEmpty()) {
        quint16 errorBitsL = m_batterystates.value(thing).bmsErrorCodeL;
        quint16 errorBitsH = m_batterystates.value(thing).bmsErrorCodeH;
        QString errorMessage{""};
        if (errorBitsL == 0 && errorBitsH == 0) {
            errorMessage.append("No error, everything ok.");
        } else {
            errorMessage.append("The following error bits are active: ");
            quint16 testBit{0b01};
            for (int var = 0; var < 16; ++var) {
                if (errorBitsL & (testBit << var)) {
                    errorMessage.append(QStringLiteral("bit %1").arg(var));
                    if (var == 3) {
                        errorMessage.append(" Cell Low temperature2, ");
                    } else if (var == 4) {
                        errorMessage.append(" Charging overcurrent2, ");
                    } else if (var == 5) {
                        errorMessage.append(" Discharging, ");
                    } else if (var == 6) {
                        errorMessage.append(" Precharge fault, ");
                    } else if (var == 7) {
                        errorMessage.append(" DC bus fault, ");
                    } else if (var == 11) {
                        errorMessage.append(" Charging circuit Failure, ");
                    } else if (var == 12) {
                        errorMessage.append(" Communication failure2, ");
                    } else if (var == 13) {
                        errorMessage.append(" Cell High, ");
                    } else if (var == 14) {
                        errorMessage.append(" Discharge, ");
                    } else if (var == 15) {
                        errorMessage.append(" Charging over-voltage3, ");
                    } else {
                        errorMessage.append(", ");
                    }
                }
            }
            for (int var = 0; var < 16; ++var) {
                if (errorBitsH & (testBit << var)) {
                    errorMessage.append(QStringLiteral("bit %1, ").arg(var + 16));
                }
            }
            int stringLength = errorMessage.length();
            if (stringLength > 1) { // stringLength should be > 1, but just in case.
                errorMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
            }
        }
        qCDebug(dcGoodwe()) << errorMessage;
        batteryThings.first()->setStateValue(goodweBatteryErrorMessageStateTypeId, errorMessage);
    }
}
