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

        if (m_meterconnected.contains(thing))
            m_meterconnected.remove(thing);

        if (m_batterypowersign.contains(thing))
            m_batterypowersign.remove(thing);

        GoodweModbusRtuConnection *connection = new GoodweModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &GoodweModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(goodweInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                m_meterconnected.find(thing)->modbusReachable = reachable;
                if (reachable && m_meterconnected.value(thing).meterCommStatus) {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, false);
                }
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                foreach (Thing *batteryThing, batteryThings) {
                    batteryThing->setStateValue(goodweBatteryConnectedStateTypeId, reachable);
                }
            }

            if (reachable) {
                qCDebug(dcGoodwe()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcGoodwe()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });


        // Handle property changed signals        
        connect(connection, &GoodweModbusRtuConnection::totalInvPowerChanged, this, [this, thing](qint16 currentPower){
            qCDebug(dcGoodwe()) << "Inverter power changed" << currentPower << "W";
            thing->setStateValue(goodweInverterRTUCurrentPowerStateTypeId, currentPower);
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
                bool commStatusBool{commStatus}
                m_meterconnected.find(thing)->meterCommStatus = commStatusBool;
                if (commStatusBool && m_meterconnected.value(thing).modbusReachable) {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, true);
                } else {
                    meterThings.first()->setStateValue(goodweMeterConnectedStateTypeId, false);
                }
            }
        });

        connect(connection, &GoodweModbusRtuConnection::acActivePowerChanged, thing, [this, thing](qint16 currentPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter power (acActivePower) changed" << currentPowerConverted << "W";
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

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerRChanged, thing, [this, thing](qint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase A (meterActivePowerR) changed" << currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseAStateTypeId, currentPowerPhaseA);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerSChanged, thing, [this, thing](qint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase B (meterActivePowerS) changed" << currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseBStateTypeId, currentPowerPhaseB);
            }
        });

        connect(connection, &GoodweModbusRtuConnection::meterActivePowerTChanged, thing, [this, thing](qint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcGoodwe()) << "Meter current power phase C (meterActivePowerT) changed" << currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(goodweMeterCurrentPowerPhaseCStateTypeId, currentPowerPhaseC);
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



        // Battery 1
        connect(connection, &GoodweModbusRtuConnection::sohBat1Changed, thing, [this, thing](quint16 sohBat1){
            qCDebug(dcGoodwe()) << "Battery 1 state of health (sohBat1) changed" << sohBat1;
            if (sohBat1 > 0) {
                // Check if w have to create the energy storage. Code supports more than one battery in principle,
                // currently only 1 though. The part ".filterByParam(goodweBatteryThingUnitParamTypeId, 1)" is
                // what enables multiple batteries.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 1);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 1 exists yet. Create it.
                    qCDebug(dcGoodwe()) << "Set up goodwe energy storage 1 for" << thing;
                    ThingDescriptor descriptor(goodweBatteryThingClassId, "Goodwe battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(goodweBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 1);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, sohBat1);
                    }
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, sohBat1);
                }
            }
        });

        connect(connection, &GoodweModbusRtuConnection::powerBat1Changed, thing, [this, thing](qint16 powerBat1){
            double powerBat1Converted = powerBat1 * 10; // currentPower state has type double.
            qCDebug(dcGoodwe()) << "Battery 1 power (powerBat1) changed" << powerBat1Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryCurrentPowerStateTypeId, powerBat1Converted);
                if (powerBat1 < 0) {    // Don't use a double to compare to 0 when you also have an int.
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &GoodweModbusRtuConnection::socBat1Changed, thing, [this, thing](quint16 socBat1){
            qCDebug(dcGoodwe()) << "Battery 1 state of charge (socBat1) changed" << socBat1 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(goodweBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }
        });


        // Battery 2. Code not active yet because registers are still missing. Have it here because now I know how it needs to be coded, and later I might not remember.
        /**
        connect(connection, &GoodweModbusRtuConnection::sohBat2Changed, thing, [this, thing](quint16 sohBat2){
            qCDebug(dcGoodwe()) << "Battery 2 state of health (sohBat2) changed" << sohBat2;
            if (sohBat2 > 0) {
                // Check if w have to create the energy storage.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 2);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 2 exists yet. Create it.
                    qCDebug(dcGoodwe()) << "Set up goodwe energy storage 2 for" << thing;
                    ThingDescriptor descriptor(goodweBatteryThingClassId, "Goodwe battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(goodweBatteryThingUnitParamTypeId, 2));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 2);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, sohBat2);
                    }
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryStateOfHealthStateTypeId, sohBat2);
                }
            }
        });

        connect(connection, &GoodweModbusRtuConnection::powerBat2Changed, thing, [this, thing](qint16 powerBat2){
            double powerBat1Converted = powerBat2 * 10; // currentPower state has type double.
            qCDebug(dcGoodwe()) << "Battery 2 power (powerBat2) changed" << powerBat2Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryCurrentPowerStateTypeId, powerBat2Converted);
                if (powerBat2 < 0) {    // Don't use a double to compare to 0 when you also have an int.
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat2 > 0) {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(goodweBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &GoodweModbusRtuConnection::socBat2Changed, thing, [this, thing](quint16 socBat2){
            qCDebug(dcGoodwe()) << "Battery 2 state of charge (socBat2) changed" << socBat2 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(goodweBatteryThingClassId).filterByParam(goodweBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(goodweBatteryBatteryLevelStateTypeId, socBat2);
                batteryThings.first()->setStateValue(goodweBatteryBatteryCriticalStateTypeId, socBat2 < 10);
            }
        });
        **/




        // FIXME: make async and check if this is really a Goodwe
        m_rtuConnections.insert(thing, connection);
        MeterConnected meterConnected{};
        m_meterconnected.insert(thing, meterConnected);
        BatteryPowerSign batteryPowerSign{};
        m_batterypowersign.insert(thing, batteryPowerSign);
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginGoodwe::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == goodweInverterTCPThingClassId || thing->thingClassId() == goodweInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcGoodwe()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(GoodweModbusTcpConnection *connection, m_tcpConnections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }

                foreach(GoodweModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginGoodwe::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        m_tcpConnections.take(thing)->deleteLater();
    }

    if (m_scalefactors.contains(thing))
        m_scalefactors.remove(thing);

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginGoodwe::setOperatingState(Thing *thing, GoodweModbusTcpConnection::OperatingState state)
{
    switch (state) {
        case GoodweModbusTcpConnection::OperatingStateOff:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Off");
            break;
        case GoodweModbusTcpConnection::OperatingStateSleeping:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Sleeping");
            break;
        case GoodweModbusTcpConnection::OperatingStateStarting:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Starting");
            break;
        case GoodweModbusTcpConnection::OperatingStateMppt:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "MPPT");
            break;
        case GoodweModbusTcpConnection::OperatingStateThrottled:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Throttled");
            break;
        case GoodweModbusTcpConnection::OperatingStateShuttingDown:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "ShuttingDown");
            break;
        case GoodweModbusTcpConnection::OperatingStateFault:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Fault");
            break;
        case GoodweModbusTcpConnection::OperatingStateStandby:
            thing->setStateValue(goodweInverterTCPOperatingStateStateTypeId, "Standby");
            break;
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
        case GoodweModbusRtuConnection::WorkModeInverterSelfCheck:
            thing->setStateValue(goodweInverterRTUOperatingStateStateTypeId, "Self check");
            break;
    }
}
