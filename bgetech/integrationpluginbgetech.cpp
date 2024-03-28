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
* SDM72 added by Consolinno Energy GmbH
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationpluginbgetech.h"
#include "plugininfo.h"
#include "discoveryrtu.h"

IntegrationPluginBGETech::IntegrationPluginBGETech()
{
}

void IntegrationPluginBGETech::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcBgeTech()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->thingClassId() == sdm630ThingClassId) {
                if (thing->paramValue(sdm630ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcBgeTech()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                    thing->setStateValue(sdm630ConnectedStateTypeId, false);
                    delete m_sdm630Connections.take(thing);
                }
            } else if (thing->thingClassId() == sdm72ThingClassId) {
                if (thing->paramValue(sdm72ThingModbusMasterUuidParamTypeId) == modbusUuid) {
                    qCWarning(dcBgeTech()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                    thing->setStateValue(sdm72ConnectedStateTypeId, false);
                    delete m_sdm72Connections.take(thing);
                }
            }
        }
    });
}

void IntegrationPluginBGETech::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == sdm630ThingClassId) {
        uint modbusId = info->params().paramValue(sdm630DiscoveryModbusIdParamTypeId).toUInt();
        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), modbusId, info);

        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery, modbusId](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
                return;
            }

            qCInfo(dcBgeTech()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {

                // Use result.meterCode() to test if this is a SDM630.
                if (result.meterCode != 112) {
                    qCDebug(dcBgeTech()) << "Found a smartmeter, but it is not an SDM630. The meter code for an SDM630 is 112. Received meter code is" << result.meterCode;
                    continue;
                }

                QString serialNumberString{QString::number(result.serialNumber)};
                ThingDescriptor descriptor(info->thingClassId(), "SDM630 Smartmeter", QString::number(modbusId) + " " + result.serialPort);

                ParamList params{
                    {sdm630ThingModbusIdParamTypeId, modbusId},
                    {sdm630ThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {sdm630ThingSerialNumberParamTypeId, serialNumberString}
                };
                descriptor.setParams(params);

                // Code for reconfigure. The discovery during reconfigure only displays things that have a ThingId that matches one of the things in myThings().
                // So we need to search myThings() for the thing that is currently reconfigured to get the ThingId, then set it.
                Things existingThings = myThings().filterByThingClassId(sdm630ThingClassId).filterByParam(sdm630ThingSerialNumberParamTypeId, serialNumberString);
                if (!existingThings.isEmpty()) {
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
        return;
    }

    if (info->thingClassId() == sdm72ThingClassId) {
        uint modbusId = info->params().paramValue(sdm72DiscoveryModbusIdParamTypeId).toUInt();
        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), modbusId, info);

        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery, modbusId](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
                return;
            }

            qCInfo(dcBgeTech()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {

                // Use result.meterCode() to test if this is a SDM72.
                if (result.meterCode != 137) {
                    qCDebug(dcBgeTech()) << "Found a smartmeter, but it is not an SDM72. The meter code for an SDM72 is 137. Received meter code is" << result.meterCode;
                    continue;
                }

                QString serialNumberString{QString::number(result.serialNumber)};
                ThingDescriptor descriptor(info->thingClassId(), "SDM72 Smartmeter", QString::number(modbusId) + " " + result.serialPort);

                ParamList params{
                    {sdm72ThingModbusIdParamTypeId, modbusId},
                    {sdm72ThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {sdm72ThingSerialNumberParamTypeId, serialNumberString}
                };
                descriptor.setParams(params);

                // Code for reconfigure. The discovery during reconfigure only displays things that have a ThingId that matches one of the things in myThings().
                // So we need to search myThings() for the thing that is currently reconfigured to get the ThingId, then set it.
                Things existingThings = myThings().filterByThingClassId(sdm72ThingClassId).filterByParam(sdm72ThingSerialNumberParamTypeId, serialNumberString);
                if (!existingThings.isEmpty()) {
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
    }
}

void IntegrationPluginBGETech::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcBgeTech()) << "Setup thing" << thing << thing->params();

    if (thing->thingClassId() == sdm630ThingClassId) {
        uint address = thing->paramValue(sdm630ThingModbusIdParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcBgeTech()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
            return;
        }

        QUuid uuid = thing->paramValue(sdm630ThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcBgeTech()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
            return;
        }

        if (m_sdm630Connections.contains(thing)) {
            qCDebug(dcBgeTech()) << "Setup after rediscovery, cleaning up ...";
            m_sdm630Connections.take(thing)->deleteLater();
        }

        Sdm630ModbusRtuConnection *sdmConnection = new Sdm630ModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(info, &ThingSetupInfo::aborted, sdmConnection, [=](){
            qCDebug(dcBgeTech()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
            sdmConnection->deleteLater();
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::reachableChanged, thing, [sdmConnection, thing](bool reachable){
            if (reachable) {
                qCDebug(dcBgeTech()) << "Modbus RTU resource " << thing << "connected on" << sdmConnection->modbusRtuMaster()->serialPort() << "is sending data.";
                sdmConnection->initialize();
            } else {
                thing->setStateValue(sdm630ConnectedStateTypeId, false);
                qCDebug(dcBgeTech()) << "Modbus RTU resource " << thing << "connected on" << sdmConnection->modbusRtuMaster()->serialPort() << "is not responding.";
                thing->setStateValue(sdm630CurrentPowerStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPhaseAStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPhaseBStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPhaseCStateTypeId, 0);
                thing->setStateValue(sdm630VoltagePhaseAStateTypeId, 0);
                thing->setStateValue(sdm630VoltagePhaseBStateTypeId, 0);
                thing->setStateValue(sdm630VoltagePhaseCStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPowerPhaseAStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPowerPhaseBStateTypeId, 0);
                thing->setStateValue(sdm630CurrentPowerPhaseCStateTypeId, 0);
                thing->setStateValue(sdm630FrequencyStateTypeId, 0);
            }
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::initializationFinished, thing, [sdmConnection, thing](bool success){
            if (success) {
                thing->setStateValue(sdm630ConnectedStateTypeId, true);


                // Disabling the auto-standby as it will shut down modbus
                //connection->setStandby(AmperfiedModbusRtuConnection::StandbyStandbyDisabled);
            } else {
                // Do some connection handling here? Timer to try again later? Stop sending modbus calls until timer triggers?
            }
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::initializationFinished, info, [this, info, sdmConnection](bool success){
            if (success) {
                if (sdmConnection->meterCode() != 112) {
                    qCWarning(dcBgeTech()) << "This does not seem to be a SDM630 smartmeter.";
                    info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("This does not seem to be a SDM630 smartmeter. Please reconfigure the device."));
                    return;
                }
                m_sdm630Connections.insert(info->thing(), sdmConnection);
                info->finish(Thing::ThingErrorNoError);
                sdmConnection->update();
            } else {
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The SDM630 smartmeter is not responding"));
            }
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
            thing->setStateValue(sdm630CurrentPhaseAStateTypeId, currentPhaseA);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
            thing->setStateValue(sdm630CurrentPhaseBStateTypeId, currentPhaseB);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
            thing->setStateValue(sdm630CurrentPhaseCStateTypeId, currentPhaseC);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
            thing->setStateValue(sdm630VoltagePhaseAStateTypeId, voltagePhaseA);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
            thing->setStateValue(sdm630VoltagePhaseBStateTypeId, voltagePhaseB);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
            thing->setStateValue(sdm630VoltagePhaseCStateTypeId, voltagePhaseC);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float currentPower){
            thing->setStateValue(sdm630CurrentPowerStateTypeId, currentPower);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
            thing->setStateValue(sdm630CurrentPowerPhaseAStateTypeId, powerPhaseA);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
            thing->setStateValue(sdm630CurrentPowerPhaseBStateTypeId, powerPhaseB);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
            thing->setStateValue(sdm630CurrentPowerPhaseCStateTypeId, powerPhaseC);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::frequencyChanged, this, [=](float frequency){
            thing->setStateValue(sdm630FrequencyStateTypeId, frequency);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float totalEnergyConsumed){
            if (totalEnergyConsumed < thing->stateValue(sdm630TotalEnergyConsumedStateTypeId).toFloat()) {
                qCWarning(dcBgeTech()) << "Total energy consumed value is smaller than the previous value. Skipping value.";
                return;
            }
            thing->setStateValue(sdm630TotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float totalEnergyProduced){
            if (totalEnergyProduced < thing->stateValue(sdm630TotalEnergyProducedStateTypeId).toFloat()) {
                qCWarning(dcBgeTech()) << "Total energy produced value is smaller than the previous value. Skipping value.";
                return;
            }
            thing->setStateValue(sdm630TotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyProducedPhaseAChanged, this, [=](float energyProducedPhaseA){
            thing->setStateValue(sdm630EnergyProducedPhaseAStateTypeId, energyProducedPhaseA);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyProducedPhaseBChanged, this, [=](float energyProducedPhaseB){
            thing->setStateValue(sdm630EnergyProducedPhaseBStateTypeId, energyProducedPhaseB);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyProducedPhaseCChanged, this, [=](float energyProducedPhaseC){
            thing->setStateValue(sdm630EnergyProducedPhaseCStateTypeId, energyProducedPhaseC);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyConsumedPhaseAChanged, this, [=](float energyConsumedPhaseA){
            thing->setStateValue(sdm630EnergyConsumedPhaseAStateTypeId, energyConsumedPhaseA);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyConsumedPhaseBChanged, this, [=](float energyConsumedPhaseB){
            thing->setStateValue(sdm630EnergyConsumedPhaseBStateTypeId, energyConsumedPhaseB);
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::energyConsumedPhaseCChanged, this, [=](float energyConsumedPhaseC){
            thing->setStateValue(sdm630EnergyConsumedPhaseCStateTypeId, energyConsumedPhaseC);
        });

    } else if (thing->thingClassId() == sdm72ThingClassId) {
        uint address = thing->paramValue(sdm72ThingModbusIdParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcBgeTech()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
            return;
        }

        QUuid uuid = thing->paramValue(sdm72ThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcBgeTech()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface not available."));
            return;
        }

        if (m_sdm72Connections.contains(thing)) {
            qCDebug(dcBgeTech()) << "Setup after rediscovery, cleaning up ...";
            m_sdm72Connections.take(thing)->deleteLater();
        }

        Sdm72ModbusRtuConnection *sdmConnection = new Sdm72ModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(sdmConnection->modbusRtuMaster(), &ModbusRtuMaster::connectedChanged, this, [=](bool connected){
            if (connected) {
                qCDebug(dcBgeTech()) << "Modbus RTU resource connected" << thing << sdmConnection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcBgeTech()) << "Modbus RTU resource disconnected" << thing << sdmConnection->modbusRtuMaster()->serialPort();
            }
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
            thing->setStateValue(sdm72CurrentPhaseAStateTypeId, currentPhaseA);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
            thing->setStateValue(sdm72CurrentPhaseBStateTypeId, currentPhaseB);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
            thing->setStateValue(sdm72CurrentPhaseCStateTypeId, currentPhaseC);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
            thing->setStateValue(sdm72VoltagePhaseAStateTypeId, voltagePhaseA);
            thing->setStateValue(sdm72ConnectedStateTypeId, true);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
            thing->setStateValue(sdm72VoltagePhaseBStateTypeId, voltagePhaseB);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
            thing->setStateValue(sdm72VoltagePhaseCStateTypeId, voltagePhaseC);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::totalCurrentPowerChanged, this, [=](float currentPower){
            thing->setStateValue(sdm72CurrentPowerStateTypeId, currentPower);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
            thing->setStateValue(sdm72CurrentPowerPhaseAStateTypeId, powerPhaseA);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
            thing->setStateValue(sdm72CurrentPowerPhaseBStateTypeId, powerPhaseB);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
            thing->setStateValue(sdm72CurrentPowerPhaseCStateTypeId, powerPhaseC);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::frequencyChanged, this, [=](float frequency){
            thing->setStateValue(sdm72FrequencyStateTypeId, frequency);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::totalEnergyConsumedChanged, this, [=](float totalEnergyConsumed){
            thing->setStateValue(sdm72TotalEnergyConsumedStateTypeId, totalEnergyConsumed);
        });

        connect(sdmConnection, &Sdm72ModbusRtuConnection::totalEnergyProducedChanged, this, [=](float totalEnergyProduced){
            thing->setStateValue(sdm72TotalEnergyProducedStateTypeId, totalEnergyProduced);
        });

        // FIXME: try to read before setup success
        m_sdm72Connections.insert(thing, sdmConnection);
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginBGETech::postSetupThing(Thing *thing)
{
    qCDebug(dcBgeTech) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach (Thing *thing, myThings()) {
                if (thing->thingClassId() == sdm630ThingClassId) {
                    m_sdm630Connections.value(thing)->update();
                } else if (thing->thingClassId() == sdm72ThingClassId) {
                    m_sdm72Connections.value(thing)->update();
                }
            }
        });

        qCDebug(dcBgeTech()) << "Starting refresh timer...";
        m_refreshTimer->start();
    }
}

void IntegrationPluginBGETech::thingRemoved(Thing *thing)
{
    qCDebug(dcBgeTech()) << "Thing removed" << thing->name();

    if (m_sdm630Connections.contains(thing))
        m_sdm630Connections.take(thing)->deleteLater();

    if (m_sdm72Connections.contains(thing))
        m_sdm72Connections.take(thing)->deleteLater();

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcBgeTech()) << "Stopping reconnect timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}
