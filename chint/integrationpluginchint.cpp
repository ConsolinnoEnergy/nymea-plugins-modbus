
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
                     QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
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
            thing->setStateValue(dtsu666FrequencyStateTypeId, 0);
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::initializationFinished,
            thing, [this, info, dtsuConnection](bool success)
    {
        if (success)
        {
            m_dtsu666Connections.insert(info->thing(), dtsuConnection);
            info->finish(Thing::ThingErrorNoError);
            dtsuConnection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The DTSU666 smartmeter is not responding."));
        }
    });

    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseAChanged, this, [=](float currentPhaseA){
        thing->setStateValue(dtsu666CurrentPhaseAStateTypeId, currentPhaseA / 1000.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseBChanged, this, [=](float currentPhaseB){
        thing->setStateValue(dtsu666CurrentPhaseBStateTypeId, currentPhaseB / 1000.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::currentPhaseCChanged, this, [=](float currentPhaseC){
        thing->setStateValue(dtsu666CurrentPhaseCStateTypeId, currentPhaseC / 1000.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseAChanged, this, [=](float voltagePhaseA){
        thing->setStateValue(dtsu666VoltagePhaseAStateTypeId, voltagePhaseA / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseBChanged, this, [=](float voltagePhaseB){
        thing->setStateValue(dtsu666VoltagePhaseBStateTypeId, voltagePhaseB / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::voltagePhaseCChanged, this, [=](float voltagePhaseC){
        thing->setStateValue(dtsu666VoltagePhaseCStateTypeId, voltagePhaseC / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerTotalChanged, this, [=](float currentPower){
        thing->setStateValue(dtsu666CurrentPowerStateTypeId, currentPower / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseAChanged, this, [=](float powerPhaseA){
        thing->setStateValue(dtsu666CurrentPowerPhaseAStateTypeId, powerPhaseA / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseBChanged, this, [=](float powerPhaseB){
        thing->setStateValue(dtsu666CurrentPowerPhaseBStateTypeId, powerPhaseB / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::powerPhaseCChanged, this, [=](float powerPhaseC){
        thing->setStateValue(dtsu666CurrentPowerPhaseCStateTypeId, powerPhaseC / 10.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::frequencyChanged, this, [=](float frequency){
        thing->setStateValue(dtsu666FrequencyStateTypeId, frequency / 100.f);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::totalForwardActiveEnergyChanged, this, [=](float totalForwardActiveEnergy){
        thing->setStateValue(dtsu666TotalEnergyConsumedStateTypeId, totalForwardActiveEnergy);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseAChanged, this, [=](float forwardActiveEnergyPhaseA){
        thing->setStateValue(dtsu666EnergyConsumedPhaseAStateTypeId, forwardActiveEnergyPhaseA);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseBChanged, this, [=](float forwardActiveEnergyPhaseB){
        thing->setStateValue(dtsu666EnergyConsumedPhaseBStateTypeId, forwardActiveEnergyPhaseB);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::forwardActiveEnergyPhaseCChanged, this, [=](float forwardActiveEnergyPhaseC){
        thing->setStateValue(dtsu666EnergyConsumedPhaseCStateTypeId, forwardActiveEnergyPhaseC);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::totalReverseActiveEnergyChanged, this, [=](float totalReverseActiveEnergy){
        thing->setStateValue(dtsu666TotalEnergyProducedStateTypeId, totalReverseActiveEnergy);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseAChanged, this, [=](float reverseActiveEnergyPhaseA){
        thing->setStateValue(dtsu666EnergyProducedPhaseAStateTypeId, reverseActiveEnergyPhaseA);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseBChanged, this, [=](float reverseActiveEnergyPhaseB){
        thing->setStateValue(dtsu666EnergyProducedPhaseBStateTypeId, reverseActiveEnergyPhaseB);
    });
    connect(dtsuConnection, &DTSU666ModbusRtuConnection::reverseActiveEnergyPhaseCChanged, this, [=](float reverseActiveEnergyPhaseC){
        thing->setStateValue(dtsu666EnergyProducedPhaseCStateTypeId, reverseActiveEnergyPhaseC);
    });
}

void IntegrationPluginChint::postSetupThing(Thing *thing)
{
    qCDebug(dcChint) << "Post setup thing" << thing->name();
    if (!m_refreshTimer) {
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            // #TODO Check for thing class ID needed here?
            // Or does myThings only contain things from this plugin (which would be only DTSU666 things in this case)?
            foreach (Thing *thing, myThings()) {
                if (thing->thingClassId() == dtsu666ThingClassId) {
                    m_dtsu666Connections.value(thing)->update();
                }
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

