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
            thing, [thing, dtsuConnection](bool success)
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
            dtsuConnection->update();
        } else {
            qCWarning(dcChint()) << "DTSU666 smartmeter Modbus RTU initialization failed.";
            dtsuConnection->initialize();
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::updateFinished,
            this, [this, thing]() { setStateValues(thing); });

    m_dtsu666Connections.insert(info->thing(), dtsuConnection);
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
    if (myThings().isEmpty() && m_refreshTimer) {
        qCDebug(dcChint()) << "Stopping reconnect timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

void IntegrationPluginChint::setStateValues(Thing *thing)
{
    if (!m_dtsu666Connections.contains(thing))
    {
        qCWarning(dcChint()) << "Can not find connection for thing" << thing;
        return;
    }
    const auto conn = m_dtsu666Connections.value(thing);
    const auto useTransformerRates =thing->paramValue(dtsu666ThingUseTransformerRatesParamTypeId).toBool();
    const auto currentTransformerRate = useTransformerRates ? conn->currentTransformerRate() : 1.f;
    const auto voltageTransformerRate = useTransformerRates ? conn->voltageTransformerRate() : 1.f;
    thing->setStateValue(dtsu666CurrentPhaseAStateTypeId,
                         conn->currentPhaseA() / 1000.f * currentTransformerRate);
    thing->setStateValue(dtsu666CurrentPhaseBStateTypeId,
                         conn->currentPhaseB() / 1000.f * currentTransformerRate);
    thing->setStateValue(dtsu666CurrentPhaseCStateTypeId,
                         conn->currentPhaseC() / 1000.f * currentTransformerRate);
    thing->setStateValue(dtsu666VoltagePhaseAStateTypeId,
                         conn->voltagePhaseA() / 10.f * voltageTransformerRate);
    thing->setStateValue(dtsu666VoltagePhaseBStateTypeId,
                         conn->voltagePhaseB() / 10.f * voltageTransformerRate);
    thing->setStateValue(dtsu666VoltagePhaseCStateTypeId,
                         conn->voltagePhaseC() / 10.f * voltageTransformerRate);
    thing->setStateValue(dtsu666CurrentPowerStateTypeId,
                         conn->powerTotal() / 10.f * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666CurrentPowerPhaseAStateTypeId,
                         conn->powerPhaseA() / 10.f * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666CurrentPowerPhaseBStateTypeId,
                         conn->powerPhaseB() / 10.f * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666CurrentPowerPhaseCStateTypeId,
                         conn->powerPhaseC() / 10.f * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666TotalEnergyConsumedStateTypeId,
                         conn->totalForwardActiveEnergy() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyConsumedPhaseAStateTypeId,
                         conn->forwardActiveEnergyPhaseA() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyConsumedPhaseBStateTypeId,
                         conn->forwardActiveEnergyPhaseB() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyConsumedPhaseCStateTypeId,
                         conn->forwardActiveEnergyPhaseC() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666TotalEnergyProducedStateTypeId,
                         conn->totalReverseActiveEnergy() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyProducedPhaseAStateTypeId,
                         conn->reverseActiveEnergyPhaseA() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyProducedPhaseBStateTypeId,
                         conn->reverseActiveEnergyPhaseB() * currentTransformerRate * voltageTransformerRate);
    thing->setStateValue(dtsu666EnergyProducedPhaseCStateTypeId,
                         conn->reverseActiveEnergyPhaseC() * currentTransformerRate * voltageTransformerRate);
}

