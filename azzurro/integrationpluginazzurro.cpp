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
        connect(connection, &AzzurroModbusRtuConnection::activePowerOnGridChanged, thing, [thing](qint16 activePower){
            double activePowerConverted = activePower * 10;
            qCDebug(dcAzzurro()) << "Inverter active power on grid changed" << activePowerConverted << "W";
            thing->setStateValue(azzurroInverterRTUCurrentPowerStateTypeId, activePowerConverted);
            thing->setStateValue(azzurroInverterRTUConnectedStateTypeId, true);
        });

        connect(connection, &AzzurroModbusRtuConnection::systemStatusChanged, thing, [thing](AzzurroModbusRtuConnection::SystemStatus systemStatus){
            qCDebug(dcAzzurro()) << "Inverter system status recieved" << systemStatus;
            Q_UNUSED(thing)
        });

        connect(connection, &AzzurroModbusRtuConnection::pvGenerationTotalChanged, thing, [thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = totalEnergyProduced / 10.0
            qCDebug(dcAzzurro()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(azzurroInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });


        // Meter
        connect(connection, &AzzurroModbusRtuConnection::powerMeterActivePowerChanged, thing, [this, thing](qint32 powerMeterActivePower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcAzzurro()) << "Meter power changed" << powerMeterActivePower << "W";
                // Note: > 0 -> return, < 0 consume
                meterThings.first()->setStateValue(huaweiMeterCurrentPowerStateTypeId, -powerMeterActivePower);
            }
        });


        // Battery
        connect(connection, &AzzurroModbusRtuConnection::lunaBattery1StatusChanged, thing, [this, thing](HuaweiModbusRtuConnection::BatteryDeviceStatus lunaBattery1Status){
            qCDebug(dcAzzurro()) << "Battery 1 status changed" << lunaBattery1Status;
            if (lunaBattery1Status != HuaweiModbusRtuConnection::BatteryDeviceStatusOffline) {
                // Check if w have to create the energy storage
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId);
                bool alreadySetUp = false;
                foreach (Thing *batteryThing, batteryThings) {
                    if (batteryThing->paramValue(huaweiBatteryThingUnitParamTypeId).toUInt() == 1) {
                        alreadySetUp = true;
                    }
                }

                if (!alreadySetUp) {
                    qCDebug(dcAzzurro()) << "Set up huawei energy storage 1 for" << thing;
                    ThingDescriptor descriptor(huaweiBatteryThingClassId, "Luna 2000 Battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(huaweiBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::lunaBattery1PowerChanged, thing, [this, thing](qint32 lunaBattery1Power){
            qCDebug(dcAzzurro()) << "Battery 1 power changed" << lunaBattery1Power << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryCurrentPowerStateTypeId, lunaBattery1Power);
                if (lunaBattery1Power < 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "discharging");
                } else if (lunaBattery1Power > 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &AzzurroModbusRtuConnection::lunaBattery1SocChanged, thing, [this, thing](float lunaBattery1Soc){
            qCDebug(dcAzzurro()) << "Battery 1 SOC changed" << lunaBattery1Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryBatteryLevelStateTypeId, lunaBattery1Soc);
                batteryThings.first()->setStateValue(huaweiBatteryBatteryCriticalStateTypeId, lunaBattery1Soc < 10);
            }
        });


        // FIXME: make async and check if this is really an azzurro
        m_rtuConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        return;
    }


    if (thing->thingClassId() == huaweiMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue("connected", parentThing->stateValue("connected").toBool());
        }
        return;
    }

    if (thing->thingClassId() == huaweiBatteryThingClassId) {
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
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId).isEmpty()) {
            qCDebug(dcAzzurro()) << "Set up huawei meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(huaweiMeterThingClassId, "Huawei Power Meter", QString(), thing->id()));
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
