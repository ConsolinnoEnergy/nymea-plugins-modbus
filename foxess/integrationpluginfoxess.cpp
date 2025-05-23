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
            if (thing->paramValue(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcFoxess()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(kacosunspecInverterRTUConnectedStateTypeId, false);
                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginFoxESS::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == kacosunspecInverterRTUThingClassId) {
        qCDebug(dcFoxess()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint modbusId = info->params().paramValue(kacosunspecInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (modbusId > 247 || modbusId < 3) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 3 and 247."));
            return;
        }

        QList<ModbusRtuMaster*> candidateMasters;
        foreach (ModbusRtuMaster *master, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
                candidateMasters.append(master);
        }

        foreach (ModbusRtuMaster *modbusMaster, candidateMasters) {
            qCDebug(dcFoxess()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            QUuid modbusMasterUuid{modbusMaster->modbusUuid()};

            ThingDescriptor descriptor(info->thingClassId(), "Kaco SunSpec Inverter", QString::number(modbusId) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(kacosunspecInverterRTUThingSlaveAddressParamTypeId, modbusId);
            params << Param(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);

            // Reconfigure not working when changing modbus ID right now.
            // The existing device should be detected by checking a hardware serial (analog to MAC address in the TCP case)
            // This is not possible right now, as the serial is only available when the device is connected
            // --> Only practical when a real Modbus RTU discovery is implemented
            // However, this correctly prevents the user from adding the same device twice, as the UI will
            // not show descriptors for already configured devices in the discovery list (they are only shown when using reconfigure)
            Thing *existingThing = myThings().findByParams(descriptor.params());
               if (existingThing) {
                   qCDebug(dcFoxess()) << "Found already configured inverter:" << existingThing->name() << modbusMaster->serialPort();
                   descriptor.setThingId(existingThing->id());
               } else {
                   qCDebug(dcFoxess()) << "Found Kaco SunSpec inverter on modbus master" << modbusMaster->serialPort() << "with modbus ID" << modbusId;
               }

            info->addThingDescriptor(descriptor);
        }
        qCDebug(dcFoxess()) << "Found" << info->thingDescriptors().count() << " Kaco SunSpec inverters";

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginFoxESS::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxess()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == kacosunspecInverterRTUThingClassId) {

        uint address = thing->paramValue(kacosunspecInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 3) {
            qCWarning(dcFoxess()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 3 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(kacosunspecInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcFoxess()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcFoxess()) << "Already have a Kaco SunSpec connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        if (m_scalefactors.contains(thing))
            m_scalefactors.remove(thing);

        RSeriesModbusRtuConnection *connection = new RSeriesModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &RSeriesModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(kacosunspecInverterRTUConnectedStateTypeId, reachable);
            if (reachable) {
                qCDebug(dcFoxess()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCDebug(dcFoxess()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
                thing->setStateValue(kacosunspecInverterRTUCurrentPowerStateTypeId, 0);
                thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Off");
            }
        });

        // Handle property changed signals
        connect(connection, &RSeriesModbusRtuConnection::activePowerSfChanged, this, [this, thing](qint16 activePowerSf){
            qCDebug(dcFoxess()) << "Inverter active power scale factor recieved" << activePowerSf;
            m_scalefactors.find(thing)->powerSf = activePowerSf;
        });

        connect(connection, &RSeriesModbusRtuConnection::activePowerChanged, this, [this, thing](qint16 activePower){
            double activePowerConverted = -1.0 * activePower * qPow(10, m_scalefactors.value(thing).powerSf);
            qCDebug(dcFoxess()) << "Inverter power changed" << activePowerConverted << "W";
            thing->setStateValue(kacosunspecInverterRTUCurrentPowerStateTypeId, activePowerConverted);
        });

        connect(connection, &RSeriesModbusRtuConnection::totalEnergyProducedSfChanged, this, [this, thing](qint16 totalEnergyProducedSf){
            qCDebug(dcFoxess()) << "Inverter total energy produced scale factor recieved" << totalEnergyProducedSf;
            m_scalefactors.find(thing)->energySf = totalEnergyProducedSf;
        });

        connect(connection, &RSeriesModbusRtuConnection::totalEnergyProducedChanged, this, [this, thing](quint32 totalEnergyProduced){
            double totalEnergyProducedConverted = (totalEnergyProduced * qPow(10, m_scalefactors.value(thing).energySf)) / 1000;
            qCDebug(dcFoxess()) << "Inverter total energy produced changed" << totalEnergyProducedConverted << "kWh";
            thing->setStateValue(kacosunspecInverterRTUTotalEnergyProducedStateTypeId, totalEnergyProducedConverted);
        });

        connect(connection, &RSeriesModbusRtuConnection::operatingStateChanged, this, [this, thing](RSeriesModbusRtuConnection::OperatingState operatingState){
            qCDebug(dcFoxess()) << "Inverter operating state recieved" << operatingState;
            setOperatingState(thing, operatingState);
        });


        // FIXME: make async and check if this is really a kaco sunspec
        //
        //
        m_rtuConnections.insert(thing, connection);
    }
}

void IntegrationPluginFoxESS::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == kacosunspecInverterTCPThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcFoxess()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(RSeriesModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginFoxESS::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == kacosunspecInverterRTUThingClassId && m_rtuConnections.contains(thing)) {
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
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Off");
            break;
        case RSeriesModbusRtuConnection::OperatingStateSleeping:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Sleeping");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStarting:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Starting");
            break;
        case RSeriesModbusRtuConnection::OperatingStateMppt:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "MPPT");
            break;
        case RSeriesModbusRtuConnection::OperatingStateThrottled:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Throttled");
            break;
        case RSeriesModbusRtuConnection::OperatingStateShuttingDown:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "ShuttingDown");
            break;
        case RSeriesModbusRtuConnection::OperatingStateFault:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Fault");
            break;
        case RSeriesModbusRtuConnection::OperatingStateStandby:
            thing->setStateValue(kacosunspecInverterRTUOperatingStateStateTypeId, "Standby");
            break;
    }
}
