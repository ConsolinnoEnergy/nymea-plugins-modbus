/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2022, nymea GmbH
* Contact: contact@nymea.io
*
* This fileDescriptor is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
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
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
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

        AzzurroModbusRtuConnection *connection = new AzzurroModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcAzzurro()) << "Modbus RTU resource connected" << thing << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcAzzurro()) << "Modbus RTU resource disconnected" << thing << connection->modbusRtuMaster()->serialPort();
            }
        });

        // Handle property changed signals for inverter
        // Check if this is the correct ragister for this value. Register "powerPv1" is alternative.
        connect(connection, &AzzurroModbusRtuConnection::activePowerOnGridChanged, thing, [thing](qint16 activePower){
            double activePowerConverted = activePower * 10;
            qCDebug(dcAzzurro()) << "Inverter active power (activePowerOnGrid) changed" << activePowerConverted << "W";
            thing->setStateValue(azzurroInverterRTUCurrentPowerStateTypeId, activePowerConverted);
            thing->setStateValue(azzurroInverterRTUConnectedStateTypeId, true);
        });

        connect(connection, &AzzurroModbusRtuConnection::systemStatusChanged, thing, [thing](AzzurroModbusRtuConnection::SystemStatus systemStatus){
            qCDebug(dcAzzurro()) << "Inverter system status recieved" << systemStatus;
            Q_UNUSED(thing);
        });

        connect(connection, &AzzurroModbusRtuConnection::pvGenerationTotalChanged, thing, [thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = totalEnergyProduced / 10.0;
            qCDebug(dcAzzurro()) << "Inverter total energy produced (pvGenerationTotal) changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(azzurroInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });


        // Meter
        connect(connection, &AzzurroModbusRtuConnection::activePowerPccChanged, thing, [this, thing](qint16 currentPower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPowerConverted = currentPower * 10;
                qCDebug(dcAzzurro()) << "Meter power (activePowerPcc) changed" << currentPowerConverted << "W";
                // Check if sign is correct for power to grid and power from grid.
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerStateTypeId, currentPowerConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::energySellingTotalChanged, thing, [this, thing](quint32 totalEnergyProduced){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double totalEnergyProducedConverted = totalEnergyProduced / 10.0;
                qCDebug(dcAzzurro()) << "Meter total energy produced (energySellingTotal) changed" << totalEnergyProducedConverted << "kWh";
                meterThings.first()->setStateValue(azzurroMeterTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::energyPurchaseTotalChanged, thing, [this, thing](quint32 totalEnergyConsumed){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double totalEnergyConsumedConverted = totalEnergyConsumed / 10.0;
                qCDebug(dcAzzurro()) << "Meter total energy consumed (energyPurchaseTotal) changed" << totalEnergyConsumedConverted << "kWh";
                meterThings.first()->setStateValue(azzurroMeterTotalEnergyConsumedStateTypeId, totalEnergyConsumedConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccRChanged, thing, [this, thing](quint16 currentPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPhaseAConverted = currentPhaseA / 100.0;
                qCDebug(dcAzzurro()) << "Meter current phase A (currentPccR) changed" << currentPhaseAConverted << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseAStateTypeId, currentPhaseAConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccSChanged, thing, [this, thing](quint16 currentPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPhaseBConverted = currentPhaseB / 100.0;
                qCDebug(dcAzzurro()) << "Meter current phase B (currentPccS) changed" << currentPhaseBConverted << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseBStateTypeId, currentPhaseBConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::currentPccTChanged, thing, [this, thing](quint16 currentPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPhaseCConverted = currentPhaseC / 100.0;
                qCDebug(dcAzzurro()) << "Meter current phase C (currentPccT) changed" << currentPhaseCConverted << "A";
                meterThings.first()->setStateValue(azzurroMeterCurrentPhaseCStateTypeId, currentPhaseCConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccRChanged, thing, [this, thing](quint16 currentPowerPhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPowerPhaseAConverted = currentPowerPhaseA * 10;
                qCDebug(dcAzzurro()) << "Meter current power phase A (activePowerPccR) changed" << currentPowerPhaseAConverted << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseAStateTypeId, currentPowerPhaseAConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccSChanged, thing, [this, thing](quint16 currentPowerPhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPowerPhaseBConverted = currentPowerPhaseB * 10;
                qCDebug(dcAzzurro()) << "Meter current power phase B (activePowerPccS) changed" << currentPowerPhaseBConverted << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseBStateTypeId, currentPowerPhaseBConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::activePowerPccTChanged, thing, [this, thing](quint16 currentPowerPhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double currentPowerPhaseCConverted = currentPowerPhaseC * 10;
                qCDebug(dcAzzurro()) << "Meter current power phase C (activePowerPccT) changed" << currentPowerPhaseCConverted << "W";
                meterThings.first()->setStateValue(azzurroMeterCurrentPowerPhaseCStateTypeId, currentPowerPhaseCConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseRChanged, thing, [this, thing](quint16 voltagePhaseA){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double voltagePhaseAConverted = voltagePhaseA / 10.0;
                qCDebug(dcAzzurro()) << "Meter voltage phase A (voltagePhaseR) changed" << voltagePhaseAConverted << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseAStateTypeId, voltagePhaseAConverted);
            }
        });


        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseSChanged, thing, [this, thing](quint16 voltagePhaseB){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double voltagePhaseBConverted = voltagePhaseB / 10.0;
                qCDebug(dcAzzurro()) << "Meter voltage phase B (voltagePhaseS) changed" << voltagePhaseBConverted << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseBStateTypeId, voltagePhaseBConverted);
            }
        });


        connect(connection, &AzzurroModbusRtuConnection::voltagePhaseTChanged, thing, [this, thing](quint16 voltagePhaseC){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double voltagePhaseCConverted = voltagePhaseC / 10.0;
                qCDebug(dcAzzurro()) << "Meter voltage phase C (voltagePhaseT) changed" << voltagePhaseCConverted << "V";
                meterThings.first()->setStateValue(azzurroMeterVoltagePhaseCStateTypeId, voltagePhaseCConverted);
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::frequencyGridChanged, thing, [this, thing](quint16 frequency){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroMeterThingClassId);
            if (!meterThings.isEmpty()) {
                double frequencyConverted = frequency / 100.0;
                qCDebug(dcAzzurro()) << "Meter frequency (frequencyGrid) changed" << frequencyConverted << "Hz";
                meterThings.first()->setStateValue(azzurroMeterFrequencyStateTypeId, frequencyConverted);
            }
        });



        // Battery
        connect(connection, &AzzurroModbusRtuConnection::sohBat1Changed, thing, [this, thing](quint16 sohBat1){
            qCDebug(dcAzzurro()) << "Battery 1 state of health (sohBat1) changed" << sohBat1;
            if (sohBat1 > 0) {
                // Check if w have to create the energy storage
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId);
                bool alreadySetUp = false;
                foreach (Thing *batteryThing, batteryThings) {
                    if (batteryThing->paramValue(azzurroBatteryThingUnitParamTypeId).toUInt() == 1) {
                        alreadySetUp = true;
                    }
                }

                if (!alreadySetUp) {
                    qCDebug(dcAzzurro()) << "Set up azzurro energy storage 1 for" << thing;
                    ThingDescriptor descriptor(azzurroBatteryThingClassId, "Azzurro battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(azzurroBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::powerBat1Changed, thing, [this, thing](quint16 powerBat1){
            qCDebug(dcAzzurro()) << "Battery 1 power changed" << lunaBattery1Power << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryCurrentPowerStateTypeId, lunaBattery1Power);
                if (lunaBattery1Power < 0) {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "discharging");
                } else if (lunaBattery1Power > 0) {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(azzurroBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::lunaBattery1SocChanged, thing, [this, thing](float lunaBattery1Soc){
            qCDebug(dcAzzurro()) << "Battery 1 SOC changed" << lunaBattery1Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(azzurroBatteryThingClassId).filterByParam(azzurroBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(azzurroBatteryBatteryLevelStateTypeId, lunaBattery1Soc);
                batteryThings.first()->setStateValue(azzurroBatteryBatteryCriticalStateTypeId, lunaBattery1Soc < 10);
            }
        });


        // FIXME: make async and check if this is really an azzurro
        m_rtuConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        return;
    }


    if (thing->thingClassId() == azzurroMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue("connected", parentThing->stateValue("connected").toBool());
        }
        return;
    }

    if (thing->thingClassId() == azzurroBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue("connected", parentThing->stateValue("connected").toBool());
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

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
