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

#include "integrationpluginazzurro.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

IntegrationPluginAzzurro::IntegrationPluginAzzurro()
{

}

void IntegrationPluginAzzurro::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcAzzurro()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(azzurroInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcAzzurro()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(azzurroInverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(azzurroMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    // Support for multiple batteries.
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(azzurroBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginAzzurro::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == azzurroInverterRTUThingClassId) {
        qCDebug(dcAzzurro()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(azzurroInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcAzzurro()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Azzurro Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(azzurroInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(azzurroInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginAzzurro::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAzzurro()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == azzurroInverterRTUThingClassId) {

        uint address = thing->paramValue(azzurroInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address == 0) {
            qCWarning(dcAzzurro()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(azzurroInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcAzzurro()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcAzzurro()) << "Already have a Azzurro connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_pvpower.contains(thing))
            m_pvpower.remove(thing);

        if (m_pvEnergyProducedValues.contains(thing))
            m_pvEnergyProducedValues.remove(thing);

        if (m_energyConsumedValues.contains(thing))
            m_energyConsumedValues.remove(thing);

        if (m_energyProducedValues.contains(thing))
            m_energyProducedValues.remove(thing);


        AzzurroModbusRtuConnection *connection = new AzzurroModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, thing);
        connect(info, &ThingSetupInfo::aborted, connection, [=](){
            qCDebug(dcAzzurro()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
            connection->deleteLater();
        });

        connect(connection, &AzzurroModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(azzurroInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(azzurroMeterConnectedStateTypeId, reachable);
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                foreach (Thing *batteryThing, batteryThings) {
                    batteryThing->setStateValue(azzurroBatteryConnectedStateTypeId, reachable);
                }
            }

            if (reachable) {
                qCDebug(dcAzzurro()) << "Modbus RTU device " << thing << "connected on" << connection->modbusRtuMaster()->serialPort() << "is sending data.";
            } else {
                qCWarning(dcAzzurro()) << "Modbus RTU device " << thing << "connected on" << connection->modbusRtuMaster()->serialPort() << "is not responding.";
                thing->setStateValue(azzurroInverterRTUCurrentPowerStateTypeId, 0);

                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(azzurroMeterCurrentPowerStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterVoltagePhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterVoltagePhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterVoltagePhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(azzurroMeterFrequencyStateTypeId, 0);
                }

                if (!batteryThings.isEmpty()) {
                    batteryThings.first()->setStateValue(azzurroBatteryCurrentPowerStateTypeId, 0);
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "idle");
                    batteryThings.first()->setStateValue(azzurroBatteryVoltageStateTypeId, 0);
                    batteryThings.first()->setStateValue(azzurroBatteryCurrentStateTypeId, 0);
                    batteryThings.first()->setStateValue(azzurroBatteryTemperatureStateTypeId, 0);
                }
            }
        });


        // Handle energy counters for inverter and meter. They get special treatment, because here we do outlier detection.
        connect(connection, &AzzurroModbusRtuConnection::updateFinished, thing, [connection, thing, this](){

            // Check for outliers. As a consequence of that, the value written to the state is not the most recent. It is several cycles old, depending on the window size.
            if (m_pvEnergyProducedValues.contains(thing)) {
                QList<float>& valueList = m_pvEnergyProducedValues.operator[](thing);
                valueList.append(connection->pvGenerationTotal());
                if (valueList.length() > m_windowLength) {
                    valueList.removeFirst();
                    uint centerIndex;
                    if (m_windowLength % 2 == 0) {
                        centerIndex = m_windowLength / 2;
                    } else {
                        centerIndex = (m_windowLength - 1)/ 2;
                    }
                    float testValue{valueList.at(centerIndex)};
                    if (isOutlier(valueList)) {
                        qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                    } else {
                        //qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is legit.";

                        float currentValue{thing->stateValue(azzurroInverterRTUTotalEnergyProducedStateTypeId).toFloat()};
                        if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                            //qCDebug(dcAzzurro()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                            thing->setStateValue(azzurroInverterRTUTotalEnergyProducedStateTypeId, testValue);
                        }
                    }
                }
            }

            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                if (m_energyConsumedValues.contains(thing)) {
                    QList<float>& valueList = m_energyConsumedValues.operator[](thing);
                    valueList.append(connection->energyPurchaseTotal());
                    if (valueList.length() > m_windowLength) {
                        valueList.removeFirst();
                        uint centerIndex;
                        if (m_windowLength % 2 == 0) {
                            centerIndex = m_windowLength / 2;
                        } else {
                            centerIndex = (m_windowLength - 1)/ 2;
                        }
                        float testValue{valueList.at(centerIndex)};
                        if (isOutlier(valueList)) {
                            qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                        } else {
                            //qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is legit.";

                            float currentValue{meterThings.first()->stateValue(azzurroMeterTotalEnergyConsumedStateTypeId).toFloat()};
                            if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                                //qCDebug(dcAzzurro()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                                meterThings.first()->setStateValue(azzurroMeterTotalEnergyConsumedStateTypeId, testValue);
                            }
                        }
                    }
                }

                if (m_energyProducedValues.contains(thing)) {
                    QList<float>& valueList = m_energyProducedValues.operator[](thing);
                    valueList.append(connection->energySellingTotal());
                    if (valueList.length() > m_windowLength) {
                        valueList.removeFirst();
                        uint centerIndex;
                        if (m_windowLength % 2 == 0) {
                            centerIndex = m_windowLength / 2;
                        } else {
                            centerIndex = (m_windowLength - 1)/ 2;
                        }
                        float testValue{valueList.at(centerIndex)};
                        if (isOutlier(valueList)) {
                            qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                        } else {
                            //qCDebug(dcAzzurro()) << "Outlier check: the value" << testValue << " is legit.";

                            float currentValue{meterThings.first()->stateValue(azzurroMeterTotalEnergyProducedStateTypeId).toFloat()};
                            if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                                //qCDebug(dcAzzurro()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                                meterThings.first()->setStateValue(azzurroMeterTotalEnergyProducedStateTypeId, testValue);
                            }
                        }
                    }
                }
            }
        });


        // Handle property changed signals for inverter
        connect(connection, &AzzurroModbusRtuConnection::powerPv1Changed, thing, [this, thing](quint16 powerPv1){
            m_pvpower.find(thing)->power1 = powerPv1;
            double combinedPower = powerPv1 + m_pvpower.value(thing).power2;
            qCDebug(dcAzzurro()) << "Inverter PV power 1 changed" << powerPv1 << "W. Combined power is" << combinedPower << "W";
            thing->setStateValue(azzurroInverterRTUCurrentPowerStateTypeId, -combinedPower);
        });

        connect(connection, &AzzurroModbusRtuConnection::powerPv2Changed, thing, [this, thing](quint16 powerPv2){
            m_pvpower.find(thing)->power2 = powerPv2;
            double combinedPower = powerPv2 + m_pvpower.value(thing).power1;
            qCDebug(dcAzzurro()) << "Inverter PV power 2 changed" << powerPv2 << "W. Combined power is" << combinedPower << "W";
            thing->setStateValue(azzurroInverterRTUCurrentPowerStateTypeId, -combinedPower);
        });

        connect(connection, &AzzurroModbusRtuConnection::systemStatusChanged, thing, [this, thing](AzzurroModbusRtuConnection::SystemStatus systemStatus){
            qCDebug(dcAzzurro()) << "Inverter system status recieved" << systemStatus;
            setSystemStatus(thing, systemStatus);
        });


        // Meter
        connect(connection, &AzzurroModbusRtuConnection::activePowerPccChanged, thing, [this, thing](float currentPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter power (activePowerPcc) changed" << -currentPower << "W";
                // Check if sign is correct for power to grid and power from grid.
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerStateTypeId, -currentPower);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccRChanged, thing, [this, thing](float currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current phase A (currentPccR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseAStateTypeId, currentPhaseA);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccSChanged, thing, [this, thing](float currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current phase B (currentPccS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseBStateTypeId, currentPhaseB);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccTChanged, thing, [this, thing](float currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current phase C (currentPccT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseCStateTypeId, currentPhaseC);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccRChanged, thing, [this, thing](float currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current power phase A (activePowerPccR) changed" << -currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseAStateTypeId, -currentPowerPhaseA);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccSChanged, thing, [this, thing](float currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current power phase B (activePowerPccS) changed" << -currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseBStateTypeId, -currentPowerPhaseB);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccTChanged, thing, [this, thing](float currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter current power phase C (activePowerPccT) changed" << -currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseCStateTypeId, -currentPowerPhaseC);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseRChanged, thing, [this, thing](float voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter voltage phase A (voltagePhaseR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseSChanged, thing, [this, thing](float voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter voltage phase B (voltagePhaseS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseTChanged, thing, [this, thing](float voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter voltage phase C (voltagePhaseT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::frequencyGridChanged, thing, [this, thing](float frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter frequency (frequencyGrid) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(azzurroMeterFrequencyStateTypeId, frequency);
            }
        });



        // Battery 1
        connect(connection, &AzzurroModbusRtuConnection::sohBat1Changed, thing, [this, thing](quint16 sohBat1){
            qCDebug(dcAzzurro()) << "Battery 1 state of health (sohBat1) changed" << sohBat1;
            if (sohBat1 > 0) {
                // Check if w have to create the energy storage. Code supports more than one battery in principle,
                // currently only 1 though. The part ".filterByParam(azzurroBatteryThingUnitParamTypeId, 1)" is
                // what enables multiple batteries.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 1 exists yet. Create it.
                    qCDebug(dcAzzurro()) << "Set up azzurro energy storage 1 for" << thing;
                    ThingDescriptor descriptor(azzurroBatteryThingClassId, "Azzurro battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(azzurroBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(azzurroBatteryStateOfHealthStateTypeId, sohBat1);
                    }
                } else {
                    batteryThings.first()->setStateValue(azzurroBatteryStateOfHealthStateTypeId, sohBat1);
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::powerBat1Changed, thing, [this, thing](qint16 powerBat1){
            // powerBat1 is not scaled by the Modbus class, because we need the value as an int here. The scaled value is always a float.
            double powerBat1Converted = powerBat1 * 10;
            qCDebug(dcAzzurro()) << "Battery 1 power (powerBat1) changed" << powerBat1Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryCurrentPowerStateTypeId, powerBat1Converted);
                if (powerBat1 < 0) {    // This is where we need int, not float.
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::socBat1Changed, thing, [this, thing](quint16 socBat1){
            qCDebug(dcAzzurro()) << "Battery 1 state of charge (socBat1) changed" << socBat1 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(azzurroBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::voltageBat1Changed, thing, [this, thing](float voltageBat1){
            qCDebug(dcAzzurro()) << "Battery 1 voltage changed" << voltageBat1 << "V";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryVoltageStateTypeId, voltageBat1);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentBat1Changed, thing, [this, thing](float currentBat1){
            qCDebug(dcAzzurro()) << "Battery 1 current changed" << currentBat1 << "A";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryCurrentStateTypeId, currentBat1);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::tempBat1Changed, thing, [this, thing](qint16 tempBat1){
            qCDebug(dcAzzurro()) << "Battery 1 temperature changed" << tempBat1 << "°C";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryTemperatureStateTypeId, tempBat1);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::cycleBat1Changed, thing, [this, thing](quint16 cycleBat1){
            qCDebug(dcAzzurro()) << "Battery 1 charge cycles changed" << cycleBat1;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryCyclesStateTypeId, cycleBat1);
            }
        });


        // Battery 2. Code not active yet because registers are still missing. Have it here because now I know how it needs to be coded, and later I might not remember.
        /**
        connect(connection, &AzzurroModbusRtuConnection::sohBat2Changed, thing, [this, thing](quint16 sohBat2){
            qCDebug(dcAzzurro()) << "Battery 2 state of health (sohBat2) changed" << sohBat2;
            if (sohBat2 > 0) {
                // Check if w have to create the energy storage.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 2);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 2 exists yet. Create it.
                    qCDebug(dcAzzurro()) << "Set up azzurro energy storage 2 for" << thing;
                    ThingDescriptor descriptor(azzurroBatteryThingClassId, "Azzurro battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(azzurroBatteryThingUnitParamTypeId, 2));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 2);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(azzurroBatteryStateOfHealthStateTypeId, sohBat2);
                    }
                } else {
                    batteryThings.first()->setStateValue(azzurroBatteryStateOfHealthStateTypeId, sohBat2);
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::powerBat2Changed, thing, [this, thing](qint16 powerBat2){
            double powerBat1Converted = powerBat2 * 10;
            qCDebug(dcAzzurro()) << "Battery 2 power (powerBat2) changed" << powerBat2Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryCurrentPowerStateTypeId, powerBat2Converted);
                if (powerBat2 < 0) {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat2 > 0) {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::socBat2Changed, thing, [this, thing](quint16 socBat2){
            qCDebug(dcAzzurro()) << "Battery 2 state of charge (socBat2) changed" << socBat2 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryBatteryLevelStateTypeId, socBat2);
                batteryThings.first()->setStateValue(azzurroBatteryBatteryCriticalStateTypeId, socBat2 < 10);
            }
        });
        **/


        // FIXME: make async and check if this is really an azzurro
        m_rtuConnections.insert(thing, connection);
        PvPower pvPower{};
        m_pvpower.insert(thing, pvPower);
        QList<float> pvEnergyProducedList{};
        m_pvEnergyProducedValues.insert(info->thing(), pvEnergyProducedList);
        QList<float> energyConsumedList{};
        m_energyConsumedValues.insert(info->thing(), energyConsumedList);
        QList<float> energyProducedList{};
        m_energyProducedValues.insert(info->thing(), energyProducedList);
        info->finish(Thing::ThingErrorNoError);
        return;
    }


    if (thing->thingClassId() == azzurroMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(azzurroMeterConnectedStateTypeId, parentThing->stateValue(azzurroInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }

    if (thing->thingClassId() == azzurroBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);

        // Set battery capacity from settings on restart.
        thing->setStateValue(azzurroBatteryCapacityStateTypeId, thing->setting(azzurroBatterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == azzurroBatterySettingsCapacityParamTypeId) {
                qCDebug(dcAzzurro()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(azzurroBatteryCapacityStateTypeId, value.toInt());
            }
        });

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(azzurroBatteryConnectedStateTypeId, parentThing->stateValue(azzurroInverterRTUConnectedStateTypeId).toBool());   
        }
        return;        
    }
}

void IntegrationPluginAzzurro::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == azzurroInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcAzzurro()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(AzzurroModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId).isEmpty()) {
            qCDebug(dcAzzurro()) << "Set up azzurro meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(azzurroMeterThingClassId, "Azzurro Power Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginAzzurro::thingRemoved(Thing *thing)
{
    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (m_pvpower.contains(thing))
        m_pvpower.remove(thing);

    if (m_pvEnergyProducedValues.contains(thing))
        m_pvEnergyProducedValues.remove(thing);

    if (m_energyConsumedValues.contains(thing))
        m_energyConsumedValues.remove(thing);

    if (m_energyProducedValues.contains(thing))
        m_energyProducedValues.remove(thing);

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginAzzurro::setSystemStatus(Thing *thing, AzzurroModbusRtuConnection::SystemStatus state)
{
    switch (state) {
        case AzzurroModbusRtuConnection::SystemStatusWaiting:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Waiting");
            break;
        case AzzurroModbusRtuConnection::SystemStatusCheckingGrid:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Checking Grid");
            break;
        case AzzurroModbusRtuConnection::SystemStatusGridConnected:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Grid Connected");
            break;
        case AzzurroModbusRtuConnection::SystemStatusGridUFP:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Grid UFP");
            break;
        case AzzurroModbusRtuConnection::SystemStatusEmergencyPowerSupplyEPS:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Emergency Power Supply");
            break;
        case AzzurroModbusRtuConnection::SystemStatusRecoverableFault:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Recoverable Fault");
            break;
        case AzzurroModbusRtuConnection::SystemStatusPermanentFault:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Permanent Fault");
            break;
        case AzzurroModbusRtuConnection::SystemStatusSelfCharging:
            thing->setStateValue(azzurroInverterRTUSystemStatusStateTypeId, "Self Charging");
            break;
    }
}

// This method uses the Hampel identifier (https://blogs.sas.com/content/iml/2021/06/01/hampel-filter-robust-outliers.html) to test if the value in the center of the window is an outlier or not.
// The input is a list of floats that contains the window of values to look at. The method will return true if the center value of that list is an outlier according to the Hampel
// identifier. If the value is not an outlier, the method will return false.
// The center value of the list is the one at (length / 2) for even length and ((length - 1) / 2) for odd length.
bool IntegrationPluginAzzurro::isOutlier(const QList<float>& list)
{
    int const windowLength{list.length()};
    if (windowLength < 3) {
        qCWarning(dcAzzurro()) << "Outlier check not working. Not enough values in the list.";
        return true;    // Unknown if the value is an outlier, but return true to not use the value because it can't be checked.
    }

    // This is the variable you can change to tweak outlier detection. It scales the size of the range in which values are deemed not an outlier. Increase the number to increase the
    // range (less values classified as an outlier), lower the number to reduce the range (more values classified as an outlier).
    uint const hampelH{3};

    float const madNormalizeFactor{1.4826};
    //qCDebug(dcAzzurro()) << "Hampel identifier: the input list -" << list;
    QList<float> sortedList{list};
    std::sort(sortedList.begin(), sortedList.end());
    //qCDebug(dcAzzurro()) << "Hampel identifier: the sorted list -" << sortedList;
    uint medianIndex;
    if (windowLength % 2 == 0) {
        medianIndex = windowLength / 2;
    } else {
        medianIndex = (windowLength - 1)/ 2;
    }
    float const median{sortedList.at(medianIndex)};
    //qCDebug(dcAzzurro()) << "Hampel identifier: the median -" << median;

    QList<float> madList;
    for (int i = 0; i < windowLength; ++i) {
        madList.append(std::abs(median - sortedList.at(i)));
    }
    //qCDebug(dcAzzurro()) << "Hampel identifier: the mad list -" << madList;

    std::sort(madList.begin(), madList.end());
    //qCDebug(dcAzzurro()) << "Hampel identifier: the sorted mad list -" << madList;
    float const hampelIdentifier{hampelH * madNormalizeFactor * madList.at(medianIndex)};
    //qCDebug(dcAzzurro()) << "Hampel identifier: the calculated Hampel identifier" << hampelIdentifier;

    bool isOutlier{std::abs(list.at(medianIndex) - median) > hampelIdentifier};
    //qCDebug(dcAzzurro()) << "Hampel identifier: the value" << list.at(medianIndex) << " is an outlier?" << isOutlier;

    return isOutlier;
}
