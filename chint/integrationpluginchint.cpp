/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2025, nymea GmbH
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "integrationpluginchint.h"
#include "plugininfo.h"
#include "discoveryrtu.h"

IntegrationPluginChint::IntegrationPluginChint()
{
}

void IntegrationPluginChint::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved,
            this, [=] (const QUuid &modbusUuid)
    {
        qCDebug(dcChint()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->thingClassId() == dtsu666ThingClassId &&
                    thing->paramValue(dtsu666ThingModbusMasterUuidParamTypeId) == modbusUuid)
            {
                qCWarning(dcChint())
                        << "Modbus RTU hardware resource removed for"
                        << thing
                        << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(dtsu666ConnectedStateTypeId, false);
                delete m_dtsu666Connections.take(thing);
                m_dtsu666Data.remove(thing);
            }
        }
    });
}

void IntegrationPluginChint::discoverThings(ThingDiscoveryInfo *info)
{
    uint modbusId = info->params().paramValue(dtsu666DiscoveryModbusIdParamTypeId).toUInt();
    DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), modbusId, info);

    connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery, modbusId](bool modbusMasterAvailable){
        if (!modbusMasterAvailable) {
            info->finish(Thing::ThingErrorHardwareNotAvailable,
                         QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
            return;
        }

        qCInfo(dcChint()) << "Discovery results:" << discovery->discoveryResults().count();

        foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {
            QString name = supportedThings().findById(info->thingClassId()).displayName();
            ThingDescriptor descriptor(info->thingClassId(), name, QString::number(modbusId) + " " + result.serialPort);

            ParamList params{
                {dtsu666ThingModbusIdParamTypeId, modbusId},
                {dtsu666ThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId}
            };
            descriptor.setParams(params);

            // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
            // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
            // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
            Things existingThings = myThings()
                    .filterByThingClassId(dtsu666ThingClassId)
                    .filterByParam(dtsu666ThingModbusIdParamTypeId, modbusId)
                    .filterByParam(dtsu666ThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId);
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

void IntegrationPluginChint::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcChint()) << "Setup thing" << thing << thing->params();

    uint address = thing->paramValue(dtsu666ThingModbusIdParamTypeId).toUInt();
    if (address > 247 || address == 0) {
        qCWarning(dcChint()) << "Setup failed, slave address is not valid" << address;
        info->finish(Thing::ThingErrorSetupFailed,
                     QT_TR_NOOP("The Modbus address is not valid. It must be a value between 1 and 247."));
        return;
    }

    QUuid uuid = thing->paramValue(dtsu666ThingModbusMasterUuidParamTypeId).toUuid();
    if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
        qCWarning(dcChint()) << "Setup failed, hardware manager not available";
        info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU interface is not available."));
        return;
    }

    if (m_dtsu666Connections.contains(thing)) {
        qCDebug(dcChint()) << "Setup after rediscovery, cleaning up ...";
        m_dtsu666Connections.take(thing)->deleteLater();
    }

    if (m_dtsu666Data.contains(thing)) {
        m_dtsu666Data.remove(thing);
    }

    if (m_energyConsumedValues.contains(thing))
        m_energyConsumedValues.remove(thing);

    if (m_energyProducedValues.contains(thing))
        m_energyProducedValues.remove(thing);

    DTSU666ModbusRtuConnection *dtsuConnection =
            new DTSU666ModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid),
                                           address,
                                           thing);
    connect(info, &ThingSetupInfo::aborted, dtsuConnection, [=] {
        qCDebug(dcChint()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
        dtsuConnection->deleteLater();
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reachableChanged,
            thing, [dtsuConnection, thing](bool reachable)
    {
        thing->setStateValue(dtsu666ConnectedStateTypeId, reachable);
        if (reachable) {
            qCDebug(dcChint())
                    << "Modbus RTU device "
                    << thing
                    << "connected on"
                    << dtsuConnection->modbusRtuMaster()->serialPort()
                    << "is sending data.";
            dtsuConnection->initialize();
        } else {
            qCDebug(dcChint())
                    << "Modbus RTU device "
                    << thing
                    << "connected on"
                    << dtsuConnection->modbusRtuMaster()->serialPort()
                    << "is not responding.";
            thing->setStateValue(dtsu666VoltagePhaseAStateTypeId, 0);
            thing->setStateValue(dtsu666VoltagePhaseBStateTypeId, 0);
            thing->setStateValue(dtsu666VoltagePhaseCStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPhaseAStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPhaseBStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPhaseCStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPowerStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPowerPhaseAStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPowerPhaseBStateTypeId, 0);
            thing->setStateValue(dtsu666CurrentPowerPhaseCStateTypeId, 0);
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::initializationFinished,
            thing, [this, thing, dtsuConnection](bool success)
    {
        if (success)
        {
            qCDebug(dcChint()) << "DTSU666 smartmeter Modbus RTU initialization successful.";
            thing->setStateValue(dtsu666CurrentTransformerRateStateTypeId,
                                 dtsuConnection->currentTransformerRate());
            thing->setStateValue(dtsu666VoltageTransformerRateStateTypeId,
                                 dtsuConnection->voltageTransformerRate());
            thing->setStateValue(dtsu666SoftwareversionStateTypeId,
                                 dtsuConnection->softwareversion());
            m_dtsu666Data[thing].currentTransformerRate = dtsuConnection->currentTransformerRate();
            m_dtsu666Data[thing].voltageTransformerRate = dtsuConnection->voltageTransformerRate();

            qCWarning(dcChint()) << "Initializing outlier detection";    
            QList<float> energyConsumedList{};
            m_energyConsumedValues.insert(thing, energyConsumedList);
            QList<float> energyProducedList{};
            m_energyProducedValues.insert(thing, energyProducedList);

            dtsuConnection->update();
        } else {
            qCWarning(dcChint()) << "DTSU666 smartmeter Modbus RTU initialization failed.";
            dtsuConnection->initialize();
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseAChanged, thing,
            [this, thing](float currentPhaseA)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPhaseAStateTypeId, currentPhaseA / 1000.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseBChanged, thing,
            [this, thing](float currentPhaseB)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPhaseBStateTypeId, currentPhaseB / 1000.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseCChanged, thing,
            [this, thing](float currentPhaseC)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPhaseCStateTypeId, currentPhaseC / 1000.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseAChanged, thing,
            [this, thing](float voltagePhaseA)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveVoltageTransformerRate();
        thing->setStateValue(dtsu666VoltagePhaseAStateTypeId, voltagePhaseA / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseBChanged, thing,
            [this, thing](float voltagePhaseB)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveVoltageTransformerRate();
        thing->setStateValue(dtsu666VoltagePhaseBStateTypeId, voltagePhaseB / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseCChanged, thing,
            [this, thing](float voltagePhaseC)
    {
        const auto factor = m_dtsu666Data[thing].getEffectiveVoltageTransformerRate();
        thing->setStateValue(dtsu666VoltagePhaseCStateTypeId, voltagePhaseC / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerTotalChanged, thing,
            [this, thing](float powerTotal)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPowerStateTypeId, powerTotal / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseAChanged, thing,
            [this, thing](float powerPhaseA)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPowerPhaseAStateTypeId, powerPhaseA / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseBChanged, thing,
            [this, thing](float powerPhaseB)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPowerPhaseBStateTypeId, powerPhaseB / 10.f * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseCChanged, thing,
            [this, thing](float powerPhaseC)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666CurrentPowerPhaseCStateTypeId, powerPhaseC / 10.f * factor);
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::totalForwardActiveEnergyChanged, thing,
            [this, thing](float totalForwardActiveEnergy)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        float newTotalValue = totalForwardActiveEnergy * factor;

        if (m_energyConsumedValues.contains(thing)) {
            QList<float>& valueList = m_energyConsumedValues.operator[](thing);
            valueList.append(newTotalValue);
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
                    qCDebug(dcChint()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                } else {
                    //qCDebug(dcChint()) << "Outlier check: the value" << testValue << " is legit.";

                    float currentValue{thing->stateValue(dtsu666TotalEnergyConsumedStateTypeId).toFloat()};
                    if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                        //qCDebug(dcChint()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                        thing->setStateValue(dtsu666TotalEnergyConsumedStateTypeId, testValue);
                    }
                }
            }
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseAChanged, thing,
            [this, thing](float forwardActiveEnergyPhaseA)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyConsumedPhaseAStateTypeId, forwardActiveEnergyPhaseA * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseBChanged, thing,
            [this, thing](float forwardActiveEnergyPhaseB)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyConsumedPhaseBStateTypeId, forwardActiveEnergyPhaseB * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseCChanged, thing,
            [this, thing](float forwardActiveEnergyPhaseC)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyConsumedPhaseCStateTypeId, forwardActiveEnergyPhaseC * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::totalReverseActiveEnergyChanged, thing,
            [this, thing](float totalReverseActiveEnergy)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        float newTotalValue = totalReverseActiveEnergy * factor;

        if (m_energyProducedValues.contains(thing)) {
            QList<float>& valueList = m_energyProducedValues.operator[](thing);
            valueList.append(newTotalValue);
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
                    qCDebug(dcChint()) << "Outlier check: the value" << testValue << " is an outlier. Sample window:" << valueList;
                } else {
                    //qCDebug(dcChint()) << "Outlier check: the value" << testValue << " is legit.";

                    float currentValue{thing->stateValue(dtsu666TotalEnergyProducedStateTypeId).toFloat()};
                    if (testValue != currentValue) {    // Yes, we are comparing floats here! This is one of the rare cases where you can actually do that. Tested, works as intended.
                        //qCDebug(dcChint()) << "Outlier check: the new value is different than the current value (" << currentValue << "). Writing new value to state.";
                        thing->setStateValue(dtsu666TotalEnergyProducedStateTypeId, testValue);
                    }
                }
            }
        }
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseAChanged, thing,
            [this, thing](float reverseActiveEnergyPhaseA)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyProducedPhaseAStateTypeId, reverseActiveEnergyPhaseA * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseBChanged, thing,
            [this, thing](float reverseActiveEnergyPhaseB)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyProducedPhaseBStateTypeId, reverseActiveEnergyPhaseB * factor);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseCChanged, thing,
            [this, thing](float reverseActiveEnergyPhaseC)
    {
        const auto factor =
                m_dtsu666Data[thing].getEffectiveVoltageTransformerRate() *
                m_dtsu666Data[thing].getEffectiveCurrentTransformerRate();
        thing->setStateValue(dtsu666EnergyProducedPhaseCStateTypeId, reverseActiveEnergyPhaseC * factor);
    });

    m_dtsu666Connections.insert(info->thing(), dtsuConnection);
    auto dtsu666Data = DTSU666Data{};
    dtsu666Data.hasCTClamps = thing->paramValue(dtsu666ThingUseTransformerRatesParamTypeId).toBool();
    m_dtsu666Data.insert(info->thing(), dtsu666Data);
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginChint::postSetupThing(Thing *thing)
{
    qCDebug(dcChint) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach (Thing *thing, myThings()) {
                m_dtsu666Connections.value(thing)->update();
            }
        });

        qCDebug(dcChint()) << "Starting refresh timer...";
        m_refreshTimer->start();
    }
}

void IntegrationPluginChint::thingRemoved(Thing *thing)
{
    qCDebug(dcChint()) << "Thing removed" << thing->name();

    if (m_dtsu666Connections.contains(thing)) {
        m_dtsu666Connections.take(thing)->deleteLater();
    }

    if (m_dtsu666Data.contains(thing)) {
        m_dtsu666Data.remove(thing);
    }

    if (m_energyProducedValues.contains(thing))
        m_energyProducedValues.remove(thing);

    if (m_energyConsumedValues.contains(thing))
        m_energyConsumedValues.remove(thing);

    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcChint()) << "Stopping reconnect timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

float IntegrationPluginChint::DTSU666Data::getEffectiveVoltageTransformerRate() const
{
    return hasCTClamps ? voltageTransformerRate : 1.f;
}

float IntegrationPluginChint::DTSU666Data::getEffectiveCurrentTransformerRate() const
{
return hasCTClamps ? currentTransformerRate : 1.f;
}

// This method uses the Hampel identifier (https://blogs.sas.com/content/iml/2021/06/01/hampel-filter-robust-outliers.html) to test if the value in the center of the window is an outlier or not.
// The input is a list of floats that contains the window of values to look at. The method will return true if the center value of that list is an outlier according to the Hampel
// identifier. If the value is not an outlier, the method will return false.
// The center value of the list is the one at (length / 2) for even length and ((length - 1) / 2) for odd length.
bool IntegrationPluginChint::isOutlier(const QList<float>& list)
{
    int const windowLength{list.length()};
    if (windowLength < 3) {
        qCWarning(dcChint()) << "Outlier check not working. Not enough values in the list.";
        return true;    // Unknown if the value is an outlier, but return true to not use the value because it can't be checked.
    }

    // This is the variable you can change to tweak outlier detection. It scales the size of the range in which values are deemed not an outlier. Increase the number to increase the
    // range (less values classified as an outlier), lower the number to reduce the range (more values classified as an outlier).
    uint const hampelH{3};

    float const madNormalizeFactor{1.4826};
    //qCDebug(dcChint()) << "Hampel identifier: the input list -" << list;
    QList<float> sortedList{list};
    std::sort(sortedList.begin(), sortedList.end());
    //qCDebug(dcChint()) << "Hampel identifier: the sorted list -" << sortedList;
    uint medianIndex;
    if (windowLength % 2 == 0) {
        medianIndex = windowLength / 2;
    } else {
        medianIndex = (windowLength - 1)/ 2;
    }
    float const median{sortedList.at(medianIndex)};
    //qCDebug(dcChint()) << "Hampel identifier: the median -" << median;

    QList<float> madList;
    for (int i = 0; i < windowLength; ++i) {
        madList.append(std::abs(median - sortedList.at(i)));
    }
    //qCDebug(dcChint()) << "Hampel identifier: the mad list -" << madList;

    std::sort(madList.begin(), madList.end());
    //qCDebug(dcChint()) << "Hampel identifier: the sorted mad list -" << madList;
    float const hampelIdentifier{hampelH * madNormalizeFactor * madList.at(medianIndex)};
    //qCDebug(dcChint()) << "Hampel identifier: the calculated Hampel identifier" << hampelIdentifier;

    bool isOutlier{std::abs(list.at(medianIndex) - median) > hampelIdentifier};
    //qCDebug(dcChint()) << "Hampel identifier: the value" << list.at(medianIndex) << " is an outlier?" << isOutlier;

    return isOutlier;
}
