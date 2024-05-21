/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2024, Consolinno Energy GmbH
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


#include "integrationplugingrowatt.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>

#include <QDebug>
//#include <QStringList>
//#include <QJsonDocument>
#include <QNetworkInterface>

//#include <hardwaremanager.h>
//#include <modbus/modbusrtuconnection.h>
//#include <hardware/modbus/modbusrtuhardwareresource.h>
//#include <serialport.h>

IntegrationPluginGrowatt::IntegrationPluginGrowatt()
{
}

void IntegrationPluginGrowatt::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=](const QUuid &modbusUuid){
        qCDebug(dcGrowatt()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(growattInverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcGrowatt()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(growattInverterRTUConnectedStateTypeId, false);

                // Set connected state for meter
                Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
                if (!meterThings.isEmpty()) {
                    meterThings.first()->setStateValue(growattMeterConnectedStateTypeId, false);
                }

                // Set connected state for battery
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId);
                if (!batteryThings.isEmpty()) {
                    foreach (Thing *batteryThing, batteryThings) {
                        batteryThing->setStateValue(growattBatteryConnectedStateTypeId, false);
                    }
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}

void IntegrationPluginGrowatt::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == growattInverterRTUThingClassId) {
        qCDebug(dcGrowatt()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        uint slaveAddress = info->params().paramValue(growattInverterRTUDiscoverySlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 247 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 247."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcGrowatt()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Growatt Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(growattInverterRTUThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(growattInverterRTUThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
    //{
    //    // Implement Modbus RTU-specific discovery if applicable
    //}
}

void IntegrationPluginGrowatt::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcGrowatt()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == growattInverterRTUThingClassId) {
        uint address = thing->paramValue(growattInverterRTUThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address < 1) {
            qCWarning(dcGrowatt()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(growattInverterRTUThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcGrowatt()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcGrowatt()) << "Already have a Growatt connection for this thing. Cleaning up old connection and initializing new one...";
            m_rtuConnections.take(thing)->deleteLater();
        }

        GrowattModbusRtuConnection *connection = new GrowattModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
        connect(connection, &GrowattModbusRtuConnection::reachableChanged, this, [=](bool reachable){
            thing->setStateValue(growattInverterRTUConnectedStateTypeId, reachable);

            // Set connected state for meter
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterConnectedStateTypeId, reachable);
            }

            // Set connected state for battery
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId);
            if (!batteryThings.isEmpty()) {
                foreach (Thing *batteryThing, batteryThings) {
                    batteryThing->setStateValue(growattBatteryConnectedStateTypeId, reachable);
                }
            }

            if (reachable) {
                qCDebug(dcGrowatt()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();
            } else {
                qCWarning(dcGrowatt()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });

        // Handle property changed signals

        // PV DC values
        //total
        connect(connection, &GrowattModbusRtuConnection::PpvChanged, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter total PV power changed to" << value;
            thing->setStateValue(growattInverterRTUCurrentPowerStateTypeId, -value);
        });
        connect(connection, &GrowattModbusRtuConnection::Eac_totalChanged, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter total generated energy changed to" << value;
            thing->setStateValue(growattInverterRTUTotalEnergyProducedStateTypeId, value);
        });
        //PV1
        connect(connection, &GrowattModbusRtuConnection::Ppv1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV1 power changed to" << value;
            thing->setStateValue(growattInverterRTUPv1PowerStateTypeId, -value);
        });
        connect(connection, &GrowattModbusRtuConnection::Vpv1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV1 voltage changed to" << value;
            thing->setStateValue(growattInverterRTUPv1VoltageStateTypeId, value);
        });
        connect(connection, &GrowattModbusRtuConnection::Ipv1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV1 current changed to" << value;
            thing->setStateValue(growattInverterRTUPv1CurrentStateTypeId, value);
        });
        //PV2
        connect(connection, &GrowattModbusRtuConnection::Ppv2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV2 power changed to" << value;
            thing->setStateValue(growattInverterRTUPv2PowerStateTypeId, -value);
        });
        connect(connection, &GrowattModbusRtuConnection::Vpv2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV2 voltage changed to" << value;
            thing->setStateValue(growattInverterRTUPv2VoltageStateTypeId, value);
        });
        connect(connection, &GrowattModbusRtuConnection::Ipv2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter PV2 current changed to" << value;
            thing->setStateValue(growattInverterRTUPv2CurrentStateTypeId, value);
        });

        

        // Inverter AC values
        //total
        connect(connection, &GrowattModbusRtuConnection::PacChanged, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter total Power changed to" << value;
            thing->setStateValue(growattInverterRTUPowerAcOutStateTypeId, -value);
        });

        //phases
        connect(connection, &GrowattModbusRtuConnection::Iac1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter AC out L1 current changed to" << value;
            thing->setStateValue(growattInverterRTUPhaseACurrentStateTypeId, value);
        });
        connect(connection, &GrowattModbusRtuConnection::Iac2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter AC out L2 current changed to" << value;
            thing->setStateValue(growattInverterRTUPhaseBCurrentStateTypeId, value);
        });
        connect(connection, &GrowattModbusRtuConnection::Iac3Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter AC out L3 current changed to" << value;
            thing->setStateValue(growattInverterRTUPhaseCCurrentStateTypeId, value);
        });

        connect(connection, &GrowattModbusRtuConnection::Vac1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Voltage L1 changed to" << value;
            thing->setStateValue(growattInverterRTUVoltagePhaseAStateTypeId, value);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterVoltagePhaseAStateTypeId, value);
            }
        });
        connect(connection, &GrowattModbusRtuConnection::Vac2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Voltage L2 changed to" << value;
            thing->setStateValue(growattInverterRTUVoltagePhaseBStateTypeId, value);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterVoltagePhaseBStateTypeId, value);
            }
        });
        connect(connection, &GrowattModbusRtuConnection::Vac3Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Voltage L3 changed to" << value;
            thing->setStateValue(growattInverterRTUVoltagePhaseCStateTypeId, value);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterVoltagePhaseCStateTypeId, value);
            }
        });

        connect(connection, &GrowattModbusRtuConnection::Pac1Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Power L1 changed to" << value;
            thing->setStateValue(growattInverterRTUCurrentPowerPhaseAStateTypeId, -value);
        });
        connect(connection, &GrowattModbusRtuConnection::Pac2Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Power L2 changed to" << value;
            thing->setStateValue(growattInverterRTUCurrentPowerPhaseBStateTypeId, -value);
        });
        connect(connection, &GrowattModbusRtuConnection::Pac3Changed, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Inverter Power L3 changed to" << value;
            thing->setStateValue(growattInverterRTUCurrentPowerPhaseCStateTypeId, -value);
        });

        connect(connection, &GrowattModbusRtuConnection::FacChanged, this, [this, thing](double value){
            qCDebug(dcGrowatt()) << "Frequency changed to" << value;
            thing->setStateValue(growattInverterRTUFrequencyStateTypeId, value);
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterFrequencyStateTypeId, value);
            }
        });

        //meter power values
        connect(connection, &GrowattModbusRtuConnection::Ptouser_totalChanged, this, [this, thing](double value){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            qCDebug(dcGrowatt()) << "Grid supply changed to" << value;
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterCurrentPowerStateTypeId, value);
            }
        });
        connect(connection, &GrowattModbusRtuConnection::Ptogrid_totalChanged, this, [this, thing](double value){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            qCDebug(dcGrowatt()) << "Grid feed-in changed to" << value;
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterCurrentPowerStateTypeId, -value);
            }
        });

        //consumption power
        connect(connection, &GrowattModbusRtuConnection::Ptoload_totalChanged, this, [this, thing](double value){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId);
            qCDebug(dcGrowatt()) << "Consumption changed to" << value;
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(growattMeterLoadPowerStateTypeId, value);
            }
        });

        // FIXME: make async and check if this is really a Growatt
        m_rtuConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        return;
    }

    if (thing->thingClassId() == growattMeterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(growattMeterConnectedStateTypeId, parentThing->stateValue(growattInverterRTUConnectedStateTypeId).toBool());
        }
        return;
    }

    if (thing->thingClassId() == growattBatteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(growattBatteryConnectedStateTypeId, parentThing->stateValue(growattInverterRTUConnectedStateTypeId).toBool());
            thing->setStateValue(growattBatteryCapacityStateTypeId, parentThing->paramValue(growattInverterRTUThingBatteryCapacityParamTypeId).toUInt());
        }
        return;
    }
}

void IntegrationPluginGrowatt::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == growattInverterRTUThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcGrowatt()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (GrowattModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
                }
            });

            m_pluginTimer->start();
        }

        // Check if we have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(growattMeterThingClassId).isEmpty()) {
            qCDebug(dcGrowatt()) << "Set up Growatt meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(growattMeterThingClassId, "Growatt Power Meter", QString(), thing->id()));
        }

        // Check if we have to set up a child battery for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(growattBatteryThingClassId).isEmpty()) {
            qCDebug(dcGrowatt()) << "Set up Growatt battery for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(growattBatteryThingClassId, "Growatt Battery", QString(), thing->id()));
        }
    }
}

void IntegrationPluginGrowatt::thingRemoved(Thing *thing)
{
    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
