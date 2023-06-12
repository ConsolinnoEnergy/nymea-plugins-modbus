/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
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
*
* pqida_inverter added by Consolinno Energy GmbH
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationpluginaeberle.h"
#include "plugininfo.h"

IntegrationPluginAEberle::IntegrationPluginAEberle()
{
}

void IntegrationPluginAEberle::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcAEberle()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->thingClassId() == pqidaThingClassId) {
                if (thing->paramValue(pqidaThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcAEberle()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                    thing->setStateValue(pqidaConnectedStateTypeId, false);
                    delete m_pqidaConnections.take(thing);
                }
            } else if (thing->thingClassId() == pqida_inverterThingClassId) {
                if (thing->paramValue(pqida_inverterThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcAEberle()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                    thing->setStateValue(pqida_inverterConnectedStateTypeId, false);
                    delete m_pqida_inverterConnections.take(thing);
                }
            }
            else if (thing->thingClassId() == pqida_consumerThingClassId) {
                if (thing->paramValue(pqida_consumerThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcAEberle()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                    thing->setStateValue(pqida_consumerConnectedStateTypeId, false);
                    delete m_pqida_consumerConnections.take(thing);
                }
            }
        }
    });
}

void IntegrationPluginAEberle::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcAEberle()) << "Discover modbus RTU resources...";
    if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up the Modbus RTU interface first."));
        return;
    }

    if (info->thingClassId() == pqidaThingClassId) {
        uint slaveAddress = info->params().paramValue(pqidaDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 254 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 254."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcAEberle()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "PQI-DA smart (Netzanschluss)", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(pqidaThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(pqidaThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
        return;
    } else if (info->thingClassId() == pqida_inverterThingClassId) {
        uint slaveAddress = info->params().paramValue(pqida_inverterDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 254 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 254."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcAEberle()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "PQI-DA smart (Inverter)", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(pqida_inverterThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(pqida_inverterThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
        return;
    } else if (info->thingClassId() == pqida_consumerThingClassId) {
        uint slaveAddress = info->params().paramValue(pqida_consumerDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 254 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 254."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcAEberle()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "PQI-DA smart (Consumer)", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(pqida_consumerThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(pqida_consumerThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
        return;
    }
    

}

void IntegrationPluginAEberle::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAEberle()) << "Setup thing" << thing << thing->params();

    if (thing->thingClassId() == pqidaThingClassId) {
        uint address = thing->paramValue(pqidaThingSlaveAddressParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcAEberle()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
            return;
        }

        QUuid uuid = thing->paramValue(pqidaThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcAEberle()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
            return;
        }

        if (m_pqidaConnections.contains(thing)) {
            qCDebug(dcAEberle()) << "Setup after rediscovery, cleaning up ...";
            m_pqidaConnections.take(thing)->deleteLater();
        }

        PqidaModbusRtuConnection *pqidaConnection = new PqidaModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(pqidaConnection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcAEberle()) << "Modbus RTU resource connected" << thing << pqidaConnection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcAEberle()) << "Modbus RTU resource disconnected" << thing << pqidaConnection->modbusRtuMaster()->serialPort();
            }
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
            thing->setStateValue(pqidaCurrentPhaseAStateTypeId, currentPhaseA);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
            thing->setStateValue(pqidaCurrentPhaseBStateTypeId, currentPhaseB);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
            thing->setStateValue(pqidaCurrentPhaseCStateTypeId, currentPhaseC);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
            thing->setStateValue(pqidaVoltagePhaseAStateTypeId, voltagePhaseA);
            thing->setStateValue(pqidaConnectedStateTypeId, true);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
            thing->setStateValue(pqidaVoltagePhaseBStateTypeId, voltagePhaseB);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
            thing->setStateValue(pqidaVoltagePhaseCStateTypeId, voltagePhaseC);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::totalCurrentPowerChanged, this, [=](float currentPower){
            thing->setStateValue(pqidaCurrentPowerStateTypeId, currentPower);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
            thing->setStateValue(pqidaCurrentPowerPhaseAStateTypeId, powerPhaseA);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
            thing->setStateValue(pqidaCurrentPowerPhaseBStateTypeId, powerPhaseB);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
            thing->setStateValue(pqidaCurrentPowerPhaseCStateTypeId, powerPhaseC);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::frequencyChanged, this, [=](float frequency){
            thing->setStateValue(pqidaFrequencyStateTypeId, frequency);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float totalEnergyConsumed){
            thing->setStateValue(pqidaTotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::totalEnergyProducedChanged, this, [=](float totalEnergyProduced){
            thing->setStateValue(pqidaTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyProducedPhaseAChanged, this, [=](float energyProducedPhaseA){
            thing->setStateValue(pqidaEnergyProducedPhaseAStateTypeId, energyProducedPhaseA);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyProducedPhaseBChanged, this, [=](float energyProducedPhaseB){
            thing->setStateValue(pqidaEnergyProducedPhaseBStateTypeId, energyProducedPhaseB);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyProducedPhaseCChanged, this, [=](float energyProducedPhaseC){
            thing->setStateValue(pqidaEnergyProducedPhaseCStateTypeId, energyProducedPhaseC);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyConsumedPhaseAChanged, this, [=](float energyConsumedPhaseA){
            thing->setStateValue(pqidaEnergyConsumedPhaseAStateTypeId, energyConsumedPhaseA);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyConsumedPhaseBChanged, this, [=](float energyConsumedPhaseB){
            thing->setStateValue(pqidaEnergyConsumedPhaseBStateTypeId, energyConsumedPhaseB);
        });

        connect(pqidaConnection, &PqidaModbusRtuConnection::energyConsumedPhaseCChanged, this, [=](float energyConsumedPhaseC){
            thing->setStateValue(pqidaEnergyConsumedPhaseCStateTypeId, energyConsumedPhaseC);
        });

        // FIXME: try to read before setup success
        m_pqidaConnections.insert(thing, pqidaConnection);
        info->finish(Thing::ThingErrorNoError);

    } else if (thing->thingClassId() == pqida_inverterThingClassId) {
        uint address = thing->paramValue(pqida_inverterThingSlaveAddressParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcAEberle()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
            return;
        }

        QUuid uuid = thing->paramValue(pqida_inverterThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcAEberle()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
            return;
        }

        if (m_pqida_inverterConnections.contains(thing)) {
            qCDebug(dcAEberle()) << "Setup after rediscovery, cleaning up ...";
            m_pqida_inverterConnections.take(thing)->deleteLater();
        }

        PqidaModbusRtuConnection *pqida_inverterConnection = new PqidaModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(pqida_inverterConnection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcAEberle()) << "Modbus RTU resource connected" << thing << pqida_inverterConnection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcAEberle()) << "Modbus RTU resource disconnected" << thing << pqida_inverterConnection->modbusRtuMaster()->serialPort();
            }
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
            thing->setStateValue(pqida_inverterCurrentPhaseAStateTypeId, currentPhaseA);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
            thing->setStateValue(pqida_inverterCurrentPhaseBStateTypeId, currentPhaseB);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
            thing->setStateValue(pqida_inverterCurrentPhaseCStateTypeId, currentPhaseC);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
            thing->setStateValue(pqida_inverterVoltagePhaseAStateTypeId, voltagePhaseA);
            thing->setStateValue(pqida_inverterConnectedStateTypeId, true);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
            thing->setStateValue(pqida_inverterVoltagePhaseBStateTypeId, voltagePhaseB);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
            thing->setStateValue(pqida_inverterVoltagePhaseCStateTypeId, voltagePhaseC);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::totalCurrentPowerChanged, this, [=](float currentPower){
            thing->setStateValue(pqida_inverterCurrentPowerStateTypeId, currentPower);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
            thing->setStateValue(pqida_inverterCurrentPowerPhaseAStateTypeId, powerPhaseA);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
            thing->setStateValue(pqida_inverterCurrentPowerPhaseBStateTypeId, powerPhaseB);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
            thing->setStateValue(pqida_inverterCurrentPowerPhaseCStateTypeId, powerPhaseC);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::frequencyChanged, this, [=](float frequency){
            thing->setStateValue(pqida_inverterFrequencyStateTypeId, frequency);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float totalEnergyConsumed){
            thing->setStateValue(pqida_inverterTotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });

        connect(pqida_inverterConnection, &PqidaModbusRtuConnection::totalEnergyProducedChanged, this, [=](float totalEnergyProduced){
            thing->setStateValue(pqida_inverterTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        // FIXME: try to read before setup success
        m_pqida_inverterConnections.insert(thing, pqida_inverterConnection);
        info->finish(Thing::ThingErrorNoError);

    } else if (thing->thingClassId() == pqida_consumerThingClassId) {
        uint address = thing->paramValue(pqida_consumerThingSlaveAddressParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcAEberle()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
            return;
        }

        QUuid uuid = thing->paramValue(pqida_consumerThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcAEberle()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
            return;
        }

        if (m_pqida_consumerConnections.contains(thing)) {
            qCDebug(dcAEberle()) << "Setup after rediscovery, cleaning up ...";
            m_pqida_consumerConnections.take(thing)->deleteLater();
        }

        PqidaModbusRtuConnection *pqida_consumerConnection = new PqidaModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(pqida_consumerConnection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcAEberle()) << "Modbus RTU resource connected" << thing << pqida_consumerConnection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcAEberle()) << "Modbus RTU resource disconnected" << thing << pqida_consumerConnection->modbusRtuMaster()->serialPort();
            }
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
            thing->setStateValue(pqida_consumerCurrentPhaseAStateTypeId, currentPhaseA);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
            thing->setStateValue(pqida_consumerCurrentPhaseBStateTypeId, currentPhaseB);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
            thing->setStateValue(pqida_consumerCurrentPhaseCStateTypeId, currentPhaseC);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
            thing->setStateValue(pqida_consumerVoltagePhaseAStateTypeId, voltagePhaseA);
            thing->setStateValue(pqida_consumerConnectedStateTypeId, true);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
            thing->setStateValue(pqida_consumerVoltagePhaseBStateTypeId, voltagePhaseB);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
            thing->setStateValue(pqida_consumerVoltagePhaseCStateTypeId, voltagePhaseC);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::totalCurrentPowerChanged, this, [=](float currentPower){
            thing->setStateValue(pqida_consumerCurrentPowerStateTypeId, currentPower);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
            thing->setStateValue(pqida_consumerCurrentPowerPhaseAStateTypeId, powerPhaseA);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
            thing->setStateValue(pqida_consumerCurrentPowerPhaseBStateTypeId, powerPhaseB);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
            thing->setStateValue(pqida_consumerCurrentPowerPhaseCStateTypeId, powerPhaseC);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::frequencyChanged, this, [=](float frequency){
            thing->setStateValue(pqida_consumerFrequencyStateTypeId, frequency);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float totalEnergyConsumed){
            thing->setStateValue(pqida_consumerTotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });

        connect(pqida_consumerConnection, &PqidaModbusRtuConnection::totalEnergyProducedChanged, this, [=](float totalEnergyProduced){
            thing->setStateValue(pqida_consumerTotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        // FIXME: try to read before setup success
        m_pqida_consumerConnections.insert(thing, pqida_consumerConnection);
        info->finish(Thing::ThingErrorNoError);
    }

}

void IntegrationPluginAEberle::postSetupThing(Thing *thing)
{
    qCDebug(dcAEberle) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach (Thing *thing, myThings()) {
                if (thing->thingClassId() == pqidaThingClassId) {
                    m_pqidaConnections.value(thing)->update();
                } else if (thing->thingClassId() == pqida_inverterThingClassId) {
                    m_pqida_inverterConnections.value(thing)->update();
                } else if (thing->thingClassId() == pqida_consumerThingClassId) {
                    m_pqida_consumerConnections.value(thing)->update();
                }
            }
        });

        qCDebug(dcAEberle()) << "Starting refresh timer...";
        m_refreshTimer->start();
    }
}

void IntegrationPluginAEberle::thingRemoved(Thing *thing)
{
    qCDebug(dcAEberle()) << "Thing removed" << thing->name();

    if (m_pqidaConnections.contains(thing))
        m_pqidaConnections.take(thing)->deleteLater();
    
    if (m_pqida_inverterConnections.contains(thing))
        m_pqida_inverterConnections.take(thing)->deleteLater();
        
    if (m_pqida_consumerConnections.contains(thing))
        m_pqida_consumerConnections.take(thing)->deleteLater();
    

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcAEberle()) << "Stopping reconnect timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}
