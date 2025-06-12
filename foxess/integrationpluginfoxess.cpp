/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2025, Consolinno Energy GmbH
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

#include <QtMath>

#include "integrationpluginfoxess.h"
#include "plugininfo.h"

#include "rseriesdiscovery.h"

IntegrationPluginFoxESS::IntegrationPluginFoxESS()
{

}

void IntegrationPluginFoxESS::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcFoxess()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(foxRSeriesThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcFoxess()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue("connected", false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginFoxESS::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == foxRSeriesThingClassId) {
        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), 1, info);

        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
                return;
            }

            qCInfo(dcFoxess()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {

                QString name = supportedThings().findById(info->thingClassId()).displayName();
                ThingDescriptor descriptor(info->thingClassId(), name, "SN" + result.serialNumber);

                ParamList params{
                    {foxRSeriesThingSlaveAddressParamTypeId, result.modbusId},
                    {foxRSeriesThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {foxRSeriesThingSerialNumberParamTypeId, result.serialNumber},
                };
                descriptor.setParams(params);

                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Things existingThings = myThings().filterByThingClassId(foxRSeriesThingClassId).filterByParam(foxRSeriesThingSerialNumberParamTypeId, result.serialNumber);
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

void IntegrationPluginFoxESS::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxess()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxRSeriesThingClassId) {

        uint address = thing->paramValue(foxRSeriesThingSlaveAddressParamTypeId).toUInt();
        if (address > 247) {
            qCWarning(dcFoxess()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(foxRSeriesThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcFoxess()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcFoxess()) << "Already have a FoxESS SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        RSeriesModbusRtuConnection *connection = new RSeriesModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &RSeriesModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue("connected", reachable);
            if (reachable) {
                qCDebug(dcFoxess()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
                connection->initialize();
            } else {
                qCDebug(dcFoxess()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
                thing->setStateValue("currentPower", 0);
                thing->setStateValue("operatingState", "Off");
            }
        });

        connect(connection, &RSeriesModbusRtuConnection::initializationFinished, info, [info, thing, connection](bool success) {
            qCWarning(dcFoxess()) << "Setup: Initialization finished";
            if (success) {
                QString deviceModel = connection->deviceModel();
                thing->setName("FoxESS " + deviceModel);
                if (deviceModel == "R75") {
                    thing->setStateMaxValue("productionLimit", 75000);
                } else if (deviceModel == "R100") {
                    thing->setStateMaxValue("productionLimit", 100000);
                } else if (deviceModel == "R110") {
                    thing->setStateMaxValue("productionLimit", 110000);
                }
                info->finish(Thing::ThingErrorNoError);
            } else {
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The inverter is not responding."));
            }
        });

        connect(connection, &RSeriesModbusRtuConnection::initializationFinished, thing, [thing, connection](bool success) {
            qCWarning(dcFoxess()) << "Setup: Initialization finished";
            if (success) {
                thing->setStateValue("firmwareVersion", connection->firmware());
                thing->setStateValue("serialNumber", connection->serialnumber());
            }
        });

        // Handle property changed signals
        connect(connection, &RSeriesModbusRtuConnection::updateFinished, thing, [=](){
            qCDebug(dcFoxess()) << "Update finished for" << thing->name();

            // Calculate inverter energy
            qint16 producedEnergySf = connection->inverterProducedEnergySf();
            quint32 producedEnergy = connection->inverterProducedEnergy();
            double calculatedEnergy = static_cast<double> ((producedEnergy * qPow(10, producedEnergySf)) / 1000);
            thing->setStateValue("totalEnergyProduced", calculatedEnergy);
            
            // Set operating state
            setOperatingState(thing, connection->inverterState());

            // Calculate inverter power
            qint16 currentPowerSf = connection->inverterCurrentPowerSf();
            qint16 currentPower = connection->inverterCurrentPower();
            double calculatedPower = static_cast<double> (-1 * qFabs(currentPower) * qPow(10, currentPowerSf));
            thing->setStateValue("currentPower", calculatedPower);

            // Calculate inverter frequency
            qint16 frequencySf = connection->inverterFrequencySf();
            quint32 frequency = connection->inverterFrequency();
            double calculatedFrequency = static_cast<double> (frequency * qPow(10, frequencySf));
            thing->setStateValue("frequency", calculatedFrequency);

            // Calculate max production limit
            qint16 productionLimitSf = connection->productionLimitSf();
            quint16 productionLimit = connection->productionLimit();
            quint16 calculatedProductionPercent = productionLimit * qPow(10, productionLimitSf);
            thing->setStateValue("productionLimit", calculatedProductionPercent * 1000);
        });

        m_rtuConnections.insert(thing, connection);
    }
}

void IntegrationPluginFoxESS::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == foxRSeriesThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcFoxess()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(RSeriesModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginFoxESS::executeAction(ThingActionInfo *info) 
{
    Thing *thing = info->thing();
    RSeriesModbusRtuConnection *connection = m_rtuConnections.value(thing);
    if (info->action().actionTypeId() == foxRSeriesProductionLimitActionTypeId) {
        uint productionLimit = info->action().paramValue(foxRSeriesProductionLimitActionProductionLimitParamTypeId).toUInt();
        qint16 scaleFactor = -1 * connection->productionLimitSf();
        quint16 calculatedProductionLimit = (productionLimit / 1000) * qPow(10, scaleFactor);
        ModbusRtuReply *reply = connection->setProductionLimit(calculatedProductionLimit);
        connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
        connect(reply, &ModbusRtuReply::finished, this, [reply, thing, info, productionLimit]() {
            if (reply->error() == ModbusRtuReply::NoError) {
                qCDebug(dcFoxess()) << "Successfully set production limit";
                thing->setStateValue("productionLimit", productionLimit);
                info->finish(Thing::ThingErrorNoError);
            } else {
                qCWarning(dcFoxess()) << "Error setting production limit";
            }
        });
    }
}

void IntegrationPluginFoxESS::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == foxRSeriesThingClassId && m_rtuConnections.contains(thing)) {
        qCDebug(dcFoxess()) << "Removing rtu connection";
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        qCDebug(dcFoxess()) << "Deleting plugin timer";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginFoxESS::setOperatingState(Thing *thing, RSeriesModbusRtuConnection::OperatingState state)
{
    switch (state) {
        case RSeriesModbusRtuConnection::OperatingStateOff:
            thing->setStateValue("operatingState", "Off");
            break;
        case RSeriesModbusRtuConnection::OperatingStateSleeping:
            thing->setStateValue("operatingState", "Sleeping");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStarting:
            thing->setStateValue("operatingState", "Starting");
            break;
        case RSeriesModbusRtuConnection::OperatingStateMppt:
            thing->setStateValue("operatingState", "MPPT");
            break;
        case RSeriesModbusRtuConnection::OperatingStateThrottled:
            thing->setStateValue("operatingState", "Throttled");
            break;
        case RSeriesModbusRtuConnection::OperatingStateShuttingDown:
            thing->setStateValue("operatingState", "ShuttingDown");
            break;
        case RSeriesModbusRtuConnection::OperatingStateFault:
            thing->setStateValue("operatingState", "Fault");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStandby:
            thing->setStateValue("operatingState", "Standby");
            break;
    }
}
