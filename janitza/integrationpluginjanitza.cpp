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

#include "integrationpluginjanitza.h"
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

                    // Disable this line for now. Not sure if every SDM630 has meter code 112.
                    //continue;
                }

                QString serialNumberString{QString::number(result.serialNumber)};
                QString name = supportedThings().findById(info->thingClassId()).displayName();
                ThingDescriptor descriptor(info->thingClassId(), name, QString::number(modbusId) + " " + result.serialPort);

                ParamList params{
                    {sdm630ThingModbusIdParamTypeId, modbusId},
                    {sdm630ThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {sdm630ThingSerialNumberParamTypeId, serialNumberString},
                    {sdm630ThingMeterCodeParamTypeId, result.meterCode}
                };
                descriptor.setParams(params);

                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Things existingThings = myThings().filterByThingClassId(sdm630ThingClassId).filterByParam(sdm630ThingSerialNumberParamTypeId, serialNumberString);
                if (!existingThings.isEmpty()) {
                    descriptor.setThingId(existingThings.first()->id());
                }

                // Some remarks to the above code: This plugin gets copy-pasted to get the inverter and consumer SDM630. Since they are different plugins, myThings() won't contain the things
                // from these plugins. So currently you can add the same device once in each plugin.

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
        return;
    }
}

void IntegrationPluginBGETech::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcBgeTech()) << "Setup thing" << thing << thing->params();

    if (thing->thingClassId() == sdm630ThingClassId) {
        uint address = thing->paramValue(sdm630ThingModbusIdParamTypeId).toUInt();
        if (address > 247 || address == 0) {
            qCWarning(dcBgeTech()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
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

        if (m_energyConsumedValues.contains(thing))
            m_energyConsumedValues.remove(thing);

        if (m_energyProducedValues.contains(thing))
            m_energyProducedValues.remove(thing);

        Sdm630ModbusRtuConnection *sdmConnection = new Sdm630ModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, thing);
        connect(info, &ThingSetupInfo::aborted, sdmConnection, [=](){
            qCDebug(dcBgeTech()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
            sdmConnection->deleteLater();
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::reachableChanged, thing, [sdmConnection, thing](bool reachable){
            thing->setStateValue(sdm630ConnectedStateTypeId, reachable);
            if (reachable) {
                qCDebug(dcBgeTech()) << "Modbus RTU device " << thing << "connected on" << sdmConnection->modbusRtuMaster()->serialPort() << "is sending data.";
                sdmConnection->initialize();
            } else {
                qCDebug(dcBgeTech()) << "Modbus RTU device " << thing << "connected on" << sdmConnection->modbusRtuMaster()->serialPort() << "is not responding.";
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
                QString serialNumberRead{QString::number(sdmConnection->serialNumber())};
                QString serialNumberConfig{thing->paramValue(sdm630ThingSerialNumberParamTypeId).toString()};
                int stringsNotEqual = QString::compare(serialNumberRead, serialNumberConfig, Qt::CaseInsensitive);  // if strings are equal, stringsNotEqual should be 0.
                if (stringsNotEqual) {
                    // The SDM630 found is a different one than configured. We assume the SDM630 was replaced, and the new device should use this config.
                    // Step 1: update the serial number.
                    qCDebug(dcBgeTech()) << "The serial number of this device is" << serialNumberRead << ". It does not match the serial number in the config, which is"
                                         << serialNumberConfig << ". Updating config with new serial number.";
                    thing->setParamValue(sdm630ThingSerialNumberParamTypeId, serialNumberRead);

                    // Todo: Step 2: search existing things if there is one with this serial number. If yes, that thing should be deleted. Otherwise there
                    // will be undefined behaviour when using reconfigure.
                }

                // Tracking meter code is not needed longterm. This is done to gather data on the meter codes of installed devices. To see if there are only two meter codes
                // (112 for SDM630 and 137 for SDM72) and these can be used to reliably identify the type of device, or if other meter codes exist as well.
                uint meterCodeRead{sdmConnection->meterCode()};
                uint meterCodeConfig{thing->paramValue(sdm630ThingMeterCodeParamTypeId).toUInt()};
                if (meterCodeRead != meterCodeConfig) {
                    qCDebug(dcBgeTech()) << "The meter code read from this device is" << meterCodeRead << ". It does not match the meter code in the config, which is"
                                         << meterCodeConfig << ". Updating config with the new meter code.";
                    thing->setParamValue(sdm630ThingMeterCodeParamTypeId, meterCodeRead);
                }
            }
        });

        connect(sdmConnection, &Sdm630ModbusRtuConnection::initializationFinished, info, [this, info, sdmConnection](bool success){
            if (success) {
                if (sdmConnection->meterCode() != 112) {
                    qCWarning(dcBgeTech()) << "This does not seem to be a SDM630 smartmeter. The meter code for an SDM630 is 112. Received meter code is" << sdmConnection->meterCode();

                    // Disable these lines for now. Not sure if every SDM630 has meter code 112.
                    //info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("This does not seem to be a SDM630 smartmeter. This is the wrong thing for that device. You need to configure the device with the correct thing."));
                    //return;
                }
                m_sdm630Connections.insert(info->thing(), sdmConnection);
                QList<float> energyConsumedList{};
                m_energyConsumedValues.insert(info->thing(), energyConsumedList);
                QList<float> energyProducedList{};
                m_energyProducedValues.insert(info->thing(), energyProducedList);
                info->finish(Thing::ThingErrorNoError);
                sdmConnection->update();
            } else {
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The SDM630 smartmeter is not responding."));
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

        connect(sdmConnection, &Sdm630ModbusRtuConnection::updateFinished, thing, [sdmConnection, thing, this](){

            // Check for outliers. As a consequence of that, the value written to the state is not the most recent. It is several cycles old, depending on the window size.
            if (m_energyConsumedValues.contains(thing)) {
                QList<float>& valueList = m_energyConsumedValues.operator[](thing);
                valueList.append(sdmConnection->totalEnergyConsumed());
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
                        qCDebug(dcBgeTech()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                    } else {
                        //qCDebug(dcBgeTech()) << "Outlier check: the value" << testValue << " is legit.";

                        float currentValue{thing->stateValue(sdm630TotalEnergyConsumedStateTypeId).toFloat()};
                        if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                            //qCDebug(dcBgeTech()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                            thing->setStateValue(sdm630TotalEnergyConsumedStateTypeId, testValue);
                        }
                    }
                }
            }

            if (m_energyProducedValues.contains(thing)) {
                QList<float>& valueList = m_energyProducedValues.operator[](thing);
                valueList.append(sdmConnection->totalEnergyProduced());
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
                        qCDebug(dcBgeTech()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                    } else {
                        //qCDebug(dcBgeTech()) << "Outlier check: the value" << testValue << " is legit.";

                        float currentValue{thing->stateValue(sdm630TotalEnergyProducedStateTypeId).toFloat()};
                        if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                            //qCDebug(dcBgeTech()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                            thing->setStateValue(sdm630TotalEnergyProducedStateTypeId, testValue);
                        }
                    }
                }
            }

        });
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

    if (m_energyConsumedValues.contains(thing))
        m_energyConsumedValues.remove(thing);

    if (m_energyProducedValues.contains(thing))
        m_energyProducedValues.remove(thing);

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcBgeTech()) << "Stopping reconnect timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}
