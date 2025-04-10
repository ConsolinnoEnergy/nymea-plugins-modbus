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

#include "integrationpluginsofarsolar.h"
#include "plugininfo.h"
#include <iostream>

#include <network/networkdevicediscovery.h>
#include <types/param.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>

IntegrationPluginSofarsolar::IntegrationPluginSofarsolar()
{
}

void IntegrationPluginSofarsolar::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=](const QUuid &modbusUuid)
            {
        qCDebug(dcSofarsolar()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(sofarsolarInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcSofarsolar()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(sofarsolarInverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(sofarsolarMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    // Support for multiple batteries.
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(sofarsolarBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        } });
}

void IntegrationPluginSofarsolar::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == sofarsolarInverterRTUThingClassId)
    {
        qCDebug(dcSofarsolar()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty())
        {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(sofarsolarInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress == 0)
        {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters())
        {
            qCDebug(dcSofarsolar()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "SOFAR solar Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(sofarsolarInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(sofarsolarInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSofarsolar::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSofarsolar()) << "Plugin last modified on 21. 5. 2024."; // Use this to check which version of the plugin is installed on devices.
    qCDebug(dcSofarsolar()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == sofarsolarInverterRTUThingClassId)
    {

        uint address = thing->paramValue(sofarsolarInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address == 0)
        {
            qCWarning(dcSofarsolar()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(sofarsolarInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid))
        {
            qCWarning(dcSofarsolar()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing))
        {
            qCDebug(dcSofarsolar()) << "Already have a SOFAR solar connection for this thing. Cleaning up old connection and initializing new one...";
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

        SofarsolarModbusRtuConnection *connection = new SofarsolarModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, thing);
        connect(info, &ThingSetupInfo::aborted, connection, [=]()
                {
            qCDebug(dcSofarsolar()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
            connection->deleteLater(); });

        connect(connection, &SofarsolarModbusRtuConnection::reachableChanged, this, [=](bool reachable)
                {
            thing->setStateValue(sofarsolarInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(sofarsolarMeterConnectedStateTypeId, reachable);
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                foreach (Thing *batteryThing, batteryThings) {
                    batteryThing->setStateValue(sofarsolarBatteryConnectedStateTypeId, reachable);
                }
            }

            if (reachable) {
                qCDebug(dcSofarsolar()) << "Modbus RTU device " << thing << "connected on" << connection->modbusRtuMaster()->serialPort() << "is sending data.";
            } else {
                qCWarning(dcSofarsolar()) << "Modbus RTU device " << thing << "connected on" << connection->modbusRtuMaster()->serialPort() << "is not responding.";
                thing->setStateValue(sofarsolarInverterRTUCurrentPowerStateTypeId, 0);

                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseAStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseBStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseCStateTypeId, 0);
                    meterThings.first()->setStateValue(sofarsolarMeterFrequencyStateTypeId, 0);
                }

                if (!batteryThings.isEmpty()) {
                    batteryThings.first()->setStateValue(sofarsolarBatteryCurrentPowerStateTypeId, 0);
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "idle");
                    batteryThings.first()->setStateValue(sofarsolarBatteryVoltageStateTypeId, 0);
                    batteryThings.first()->setStateValue(sofarsolarBatteryCurrentStateTypeId, 0);
                    batteryThings.first()->setStateValue(sofarsolarBatteryTemperatureStateTypeId, 0);
                }
            } });

        // Handle energy counters for inverter and meter. They get special treatment, because here we do outlier detection.
        connect(connection, &SofarsolarModbusRtuConnection::updateFinished, thing, [connection, thing, this]()
                {

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
                        qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                    } else {
                        //qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is legit.";

                        float currentValue{thing->stateValue(sofarsolarInverterRTUTotalEnergyProducedStateTypeId).toFloat()};
                        if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                            //qCDebug(dcSofarsolar()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                            thing->setStateValue(sofarsolarInverterRTUTotalEnergyProducedStateTypeId, testValue);
                        }
                    }
                }
            }

            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
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
                            qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                        } else {
                            //qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is legit.";

                            float currentValue{meterThings.first()->stateValue(sofarsolarMeterTotalEnergyConsumedStateTypeId).toFloat()};
                            if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                                //qCDebug(dcSofarsolar()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                                meterThings.first()->setStateValue(sofarsolarMeterTotalEnergyConsumedStateTypeId, testValue);
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
                            qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                        } else {
                            //qCDebug(dcSofarsolar()) << "Outlier check: the value" << testValue << " is legit.";

                            float currentValue{meterThings.first()->stateValue(sofarsolarMeterTotalEnergyProducedStateTypeId).toFloat()};
                            if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                                //qCDebug(dcSofarsolar()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                                meterThings.first()->setStateValue(sofarsolarMeterTotalEnergyProducedStateTypeId, testValue);
                            }
                        }
                    }
                }
            } });

        // Handle property changed signals for inverter
        connect(connection, &SofarsolarModbusRtuConnection::powerPv1Changed, thing, [this, thing](quint16 powerPv1)
                {
            m_pvpower.find(thing)->power1 = powerPv1;
            double combinedPower = powerPv1 + m_pvpower.value(thing).power2;
            qCDebug(dcSofarsolar()) << "Inverter PV power 1 changed" << powerPv1 << "W. Combined power is" << combinedPower << "W";
            thing->setStateValue(sofarsolarInverterRTUCurrentPowerStateTypeId, -combinedPower); });

        connect(connection, &SofarsolarModbusRtuConnection::powerPv2Changed, thing, [this, thing](quint16 powerPv2)
                {
            m_pvpower.find(thing)->power2 = powerPv2;
            double combinedPower = powerPv2 + m_pvpower.value(thing).power1;
            qCDebug(dcSofarsolar()) << "Inverter PV power 2 changed" << powerPv2 << "W. Combined power is" << combinedPower << "W";
            thing->setStateValue(sofarsolarInverterRTUCurrentPowerStateTypeId, -combinedPower); });

        connect(connection, &SofarsolarModbusRtuConnection::systemStatusChanged, thing, [this, thing](SofarsolarModbusRtuConnection::SystemStatus systemStatus)
                {
            qCDebug(dcSofarsolar()) << "Inverter system status recieved" << systemStatus;
            setSystemStatus(thing, systemStatus); });

        // control values
        connect(connection, &SofarsolarModbusRtuConnection::powerControlChanged, this, [this, thing](uint value)
                {
                    uint exportLimitEnabled = value & 0x01;

                    qCDebug(dcSofarsolar()) << "Export limit enabled changed to" << exportLimitEnabled;
                    if (exportLimitEnabled == 0)
                    {
                        thing->setStateValue(sofarsolarInverterRTUExportLimitEnableStateTypeId, false);
                    }
                    else
                    {
                        thing->setStateValue(sofarsolarInverterRTUExportLimitEnableStateTypeId, true);
                    } });
        connect(connection, &SofarsolarModbusRtuConnection::activePowerExportLimitChanged, this, [this, thing](double value)
                {
            qCDebug(dcSofarsolar()) << "Export limit power rate changed to" << value;
            thing->setStateValue(sofarsolarInverterRTUExportLimitStateTypeId, value); });

        // Meter
        connect(connection, &SofarsolarModbusRtuConnection::activePowerPccChanged, thing, [this, thing](float currentPower)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter power (activePowerPcc) changed" << -currentPower << "W";
                // Check if sign is correct for power to grid and power from grid.
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerStateTypeId, -currentPower);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::currentPccRChanged, thing, [this, thing](float currentPhaseA)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current phase A (currentPccR) changed" << currentPhaseA << "A";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseAStateTypeId, currentPhaseA);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::currentPccSChanged, thing, [this, thing](float currentPhaseB)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current phase B (currentPccS) changed" << currentPhaseB << "A";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseBStateTypeId, currentPhaseB);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::currentPccTChanged, thing, [this, thing](float currentPhaseC)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current phase C (currentPccT) changed" << currentPhaseC << "A";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPhaseCStateTypeId, currentPhaseC);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::activePowerPccRChanged, thing, [this, thing](float currentPowerPhaseA)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current power phase A (activePowerPccR) changed" << -currentPowerPhaseA << "W";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseAStateTypeId, -currentPowerPhaseA);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::activePowerPccSChanged, thing, [this, thing](float currentPowerPhaseB)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current power phase B (activePowerPccS) changed" << -currentPowerPhaseB << "W";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseBStateTypeId, -currentPowerPhaseB);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::activePowerPccTChanged, thing, [this, thing](float currentPowerPhaseC)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter current power phase C (activePowerPccT) changed" << -currentPowerPhaseC << "W";
                meterThings.first()->setStateValue(sofarsolarMeterCurrentPowerPhaseCStateTypeId, -currentPowerPhaseC);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::voltagePhaseRChanged, thing, [this, thing](float voltagePhaseA)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter voltage phase A (voltagePhaseR) changed" << voltagePhaseA << "V";
                meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseAStateTypeId, voltagePhaseA);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::voltagePhaseSChanged, thing, [this, thing](float voltagePhaseB)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter voltage phase B (voltagePhaseS) changed" << voltagePhaseB << "V";
                meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseBStateTypeId, voltagePhaseB);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::voltagePhaseTChanged, thing, [this, thing](float voltagePhaseC)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter voltage phase C (voltagePhaseT) changed" << voltagePhaseC << "V";
                meterThings.first()->setStateValue(sofarsolarMeterVoltagePhaseCStateTypeId, voltagePhaseC);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::frequencyGridChanged, thing, [this, thing](float frequency)
                {
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcSofarsolar()) << "Meter frequency (frequencyGrid) changed" << frequency << "Hz";
                meterThings.first()->setStateValue(sofarsolarMeterFrequencyStateTypeId, frequency);
            } });

        // Battery 1
        connect(connection, &SofarsolarModbusRtuConnection::sohBat1Changed, thing, [this, thing](quint16 sohBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 state of health (sohBat1) changed" << sohBat1;
            if (sohBat1 > 0) {
                // Check if w have to create the energy storage. Code supports more than one battery in principle,
                // currently only 1 though. The part ".filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1)" is
                // what enables multiple batteries.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 1 exists yet. Create it.
                    qCDebug(dcSofarsolar()) << "Set up SOFAR solar energy storage 1 for" << thing;
                    ThingDescriptor descriptor(sofarsolarBatteryThingClassId, "SOFAR solar battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(sofarsolarBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(sofarsolarBatteryStateOfHealthStateTypeId, sohBat1);
                    }
                } else {
                    batteryThings.first()->setStateValue(sofarsolarBatteryStateOfHealthStateTypeId, sohBat1);
                }
            } });

        connect(connection, &SofarsolarModbusRtuConnection::powerBat1Changed, thing, [this, thing](qint16 powerBat1)
                {
            // powerBat1 is not scaled by the Modbus class, because we need the value as an int here. The scaled value is always a float.
            double powerBat1Converted = powerBat1 * 10;
            qCDebug(dcSofarsolar()) << "Battery 1 power (powerBat1) changed" << powerBat1Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryCurrentPowerStateTypeId, powerBat1Converted);
                if (powerBat1 < 0) {    // This is where we need int, not float.
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat1 > 0) {
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "idle");
                }
            } });

        connect(connection, &SofarsolarModbusRtuConnection::socBat1Changed, thing, [this, thing](quint16 socBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 state of charge (socBat1) changed" << socBat1 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryBatteryLevelStateTypeId, socBat1);
                batteryThings.first()->setStateValue(sofarsolarBatteryBatteryCriticalStateTypeId, socBat1 < 10);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::voltageBat1Changed, thing, [this, thing](float voltageBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 voltage changed" << voltageBat1 << "V";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryVoltageStateTypeId, voltageBat1);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::currentBat1Changed, thing, [this, thing](float currentBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 current changed" << currentBat1 << "A";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryCurrentStateTypeId, currentBat1);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::tempBat1Changed, thing, [this, thing](qint16 tempBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 temperature changed" << tempBat1 << "°C";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryTemperatureStateTypeId, tempBat1);
            } });

        connect(connection, &SofarsolarModbusRtuConnection::cycleBat1Changed, thing, [this, thing](quint16 cycleBat1)
                {
            qCDebug(dcSofarsolar()) << "Battery 1 charge cycles changed" << cycleBat1;
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryCyclesStateTypeId, cycleBat1);
            } });

        // Battery 2. Code not active yet because registers are still missing. Have it here because now I know how it needs to be coded, and later I might not remember.
        /**
        connect(connection, &SofarsolarModbusRtuConnection::sohBat2Changed, thing, [this, thing](quint16 sohBat2){
            qCDebug(dcSofarsolar()) << "Battery 2 state of health (sohBat2) changed" << sohBat2;
            if (sohBat2 > 0) {
                // Check if w have to create the energy storage.
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 2);
                if (batteryThings.isEmpty()) {
                    // No battery with Unit = 2 exists yet. Create it.
                    qCDebug(dcSofarsolar()) << "Set up SOFAR solar energy storage 2 for" << thing;
                    ThingDescriptor descriptor(sofarsolarBatteryThingClassId, "SOFAR solar battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(sofarsolarBatteryThingUnitParamTypeId, 2));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);

                    // Not sure if battery is already available here. Try to set state now, because otherwise have to wait for next change in
                    // soh before value will be set. Soh does not change that fast.
                    batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 2);
                    if (!batteryThings.isEmpty()) {
                        batteryThings.first()->setStateValue(sofarsolarBatteryStateOfHealthStateTypeId, sohBat2);
                    }
                } else {
                    batteryThings.first()->setStateValue(sofarsolarBatteryStateOfHealthStateTypeId, sohBat2);
                }
            }
        });

        connect(connection, &SofarsolarModbusRtuConnection::powerBat2Changed, thing, [this, thing](qint16 powerBat2){
            double powerBat1Converted = powerBat2 * 10;
            qCDebug(dcSofarsolar()) << "Battery 2 power (powerBat2) changed" << powerBat2Converted << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryCurrentPowerStateTypeId, powerBat2Converted);
                if (powerBat2 < 0) {
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "discharging");
                } else if (powerBat2 > 0) {
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(sofarsolarBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &SofarsolarModbusRtuConnection::socBat2Changed, thing, [this, thing](quint16 socBat2){
            qCDebug(dcSofarsolar()) << "Battery 2 state of charge (socBat2) changed" << socBat2 << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarBatteryThingClassId).filterByParam(sofarsolarBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(sofarsolarBatteryBatteryLevelStateTypeId, socBat2);
                batteryThings.first()->setStateValue(sofarsolarBatteryBatteryCriticalStateTypeId, socBat2 < 10);
            }
        });
        **/

        // FIXME: make async and check if this is really an SOFAR solar
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

    if (thing->thingClassId() == sofarsolarMeterThingClassId)
    {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing)
        {
            thing->setStateValue(sofarsolarMeterConnectedStateTypeId, parentThing->stateValue(sofarsolarInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }

    if (thing->thingClassId() == sofarsolarBatteryThingClassId)
    {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);

        // Set battery capacity from settings on restart.
        thing->setStateValue(sofarsolarBatteryCapacityStateTypeId, thing->setting(sofarsolarBatterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing](const ParamTypeId &paramTypeId, const QVariant &value)
                {
            if (paramTypeId == sofarsolarBatterySettingsCapacityParamTypeId) {
                qCDebug(dcSofarsolar()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(sofarsolarBatteryCapacityStateTypeId, value.toInt());
            } });

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing)
        {
            thing->setStateValue(sofarsolarBatteryConnectedStateTypeId, parentThing->stateValue(sofarsolarInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }
}

void IntegrationPluginSofarsolar::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == sofarsolarInverterRTUThingClassId)
    {
        if (!m_pluginTimer)
        {
            qCDebug(dcSofarsolar()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this]
                    {
                foreach(SofarsolarModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                } });

            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(sofarsolarMeterThingClassId).isEmpty())
        {
            qCDebug(dcSofarsolar()) << "Set up SOFAR solar meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(sofarsolarMeterThingClassId, "SOFAR solar Power Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginSofarsolar::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();

    if (thing->thingClassId() == sofarsolarInverterRTUThingClassId)
    {
        SofarsolarModbusRtuConnection *sofarsolarmodbusrtuconnection = m_rtuConnections.value(thing);

        if (!sofarsolarmodbusrtuconnection)
        {
            qCWarning(dcSofarsolar()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        /*if (!sofarsolarmodbusrtuconnection->connected()) {
            qCWarning(dcSofarsolar()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }*/

        bool success = false;
        if (info->action().actionTypeId() == sofarsolarInverterRTUExportLimitEnableActionTypeId)
        {
            // enable/disable export limit
            bool enabled = info->action().paramValue(sofarsolarInverterRTUExportLimitEnableActionExportLimitEnableParamTypeId).toBool();
            uint state = (uint)sofarsolarmodbusrtuconnection->powerControl();

            if (enabled)
            {
                state |= 0x01;
            }
            else
            {
                state &= 0xFFFE;
            }
            ModbusRtuReply *reply = sofarsolarmodbusrtuconnection->setPowerControl(state);
            success = handleReply(reply);
        }
        else if (info->action().actionTypeId() == sofarsolarInverterRTUExportLimitActionTypeId)
        {
            uint valuePercent = info->action().paramValue(sofarsolarInverterRTUExportLimitActionExportLimitParamTypeId).toInt();
            std::cout << "valuePercent: " << valuePercent << std::endl;
            ModbusRtuReply *reply = sofarsolarmodbusrtuconnection->setActivePowerExportLimit(valuePercent);
            success = handleReply(reply);
            success = true;
        }
        else
        {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
        }

        if (success)
        {
            info->finish(Thing::ThingErrorNoError);
        }
        else
        {
            qCWarning(dcSofarsolar()) << "Action execution finished with error.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }
    }
}

bool IntegrationPluginSofarsolar::handleReply(ModbusRtuReply *reply)
{
    if (!reply)
    {
        qCWarning(dcSofarsolar()) << "Sending modbus command failed because the reply could not be created.";
        // m_errorOccured = true;
        return false;
    }
    connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
    connect(reply, &ModbusRtuReply::errorOccurred, this, [reply](ModbusRtuReply::Error error)
            {
                qCWarning(dcSofarsolar()) << "Modbus reply error occurred while writing modbus" << error << reply->errorString();
                emit reply->finished(); // To make sure it will be deleted
            });
    return true;
}

void IntegrationPluginSofarsolar::thingRemoved(Thing *thing)
{
    if (m_rtuConnections.contains(thing))
    {
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

    if (myThings().isEmpty() && m_pluginTimer)
    {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSofarsolar::setSystemStatus(Thing *thing, SofarsolarModbusRtuConnection::SystemStatus state)
{
    switch (state)
    {
    case SofarsolarModbusRtuConnection::SystemStatusWaiting:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Waiting");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusDetection:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Detection");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusGridConnected:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Grid Connected");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusEmergencyPowerSupply:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Emergency Power Supply");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusRecoverableFault:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Recoverable Fault");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusPermanentFault:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Permanent Fault");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusUpgrade:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Upgrade");
        break;
    case SofarsolarModbusRtuConnection::SystemStatusSelfCharging:
        thing->setStateValue(sofarsolarInverterRTUSystemStatusStateTypeId, "Self Charging");
        break;
    }
}

// This method uses the Hampel identifier (https://blogs.sas.com/content/iml/2021/06/01/hampel-filter-robust-outliers.html) to test if the value in the center of the window is an outlier or not.
// The input is a list of floats that contains the window of values to look at. The method will return true if the center value of that list is an outlier according to the Hampel
// identifier. If the value is not an outlier, the method will return false.
// The center value of the list is the one at (length / 2) for even length and ((length - 1) / 2) for odd length.
bool IntegrationPluginSofarsolar::isOutlier(const QList<float> &list)
{
    int const windowLength{list.length()};
    if (windowLength < 3)
    {
        qCWarning(dcSofarsolar()) << "Outlier check not working. Not enough values in the list.";
        return true; // Unknown if the value is an outlier, but return true to not use the value because it can't be checked.
    }

    // This is the variable you can change to tweak outlier detection. It scales the size of the range in which values are deemed not an outlier. Increase the number to increase the
    // range (less values classified as an outlier), lower the number to reduce the range (more values classified as an outlier).
    uint const hampelH{3};

    float const madNormalizeFactor{1.4826};
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the input list -" << list;
    QList<float> sortedList{list};
    std::sort(sortedList.begin(), sortedList.end());
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the sorted list -" << sortedList;
    uint medianIndex;
    if (windowLength % 2 == 0)
    {
        medianIndex = windowLength / 2;
    }
    else
    {
        medianIndex = (windowLength - 1) / 2;
    }
    float const median{sortedList.at(medianIndex)};
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the median -" << median;

    QList<float> madList;
    for (int i = 0; i < windowLength; ++i)
    {
        madList.append(std::abs(median - sortedList.at(i)));
    }
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the mad list -" << madList;

    std::sort(madList.begin(), madList.end());
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the sorted mad list -" << madList;
    float const hampelIdentifier{hampelH * madNormalizeFactor * madList.at(medianIndex)};
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the calculated Hampel identifier" << hampelIdentifier;

    bool isOutlier{std::abs(list.at(medianIndex) - median) > hampelIdentifier};
    // qCDebug(dcSofarsolar()) << "Hampel identifier: the value" << list.at(medianIndex) << " is an outlier?" << isOutlier;

    return isOutlier;
}
