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

#include "huaweifusionsolardiscovery.h"
#include "discoveryrtu.h"
#include "integrationpluginhuawei.h"
#include "plugininfo.h"

#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <hardwaremanager.h>

IntegrationPluginHuawei::IntegrationPluginHuawei()
{

}

void IntegrationPluginHuawei::init()
{
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcHuawei()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(huaweiRtuInverterThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcHuawei()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(huaweiRtuInverterConnectedStateTypeId, false);

                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }

                delete m_rtuConnections.take(thing);
            }
        }
    });
}


void IntegrationPluginHuawei::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == huaweiFusionSolarInverterThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcHuawei()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        QList<quint16> modbusIds = {1, 2, 3};
        HuaweiFusionSolarDiscovery *discovery = new HuaweiFusionSolarDiscovery(hardwareManager()->networkDeviceDiscovery(), 502, modbusIds, info);
        connect(discovery, &HuaweiFusionSolarDiscovery::discoveryFinished, info, [=](){
            foreach (const HuaweiFusionSolarDiscovery::Result &result, discovery->results()) {

                QString name = QT_TR_NOOP("Huawei Solar Inverter");
                if (!result.modelName.isEmpty())
                    name = "Huawei " + result.modelName;

                QString desctiption = result.networkDeviceInfo.macAddress() + " - " + result.networkDeviceInfo.address().toString();
                if (!result.serialNumber.isEmpty()) {
                    desctiption = "SN: " + result.serialNumber + " " + result.networkDeviceInfo.macAddress() + " - " + result.networkDeviceInfo.address().toString();
                }

                ThingDescriptor descriptor(huaweiFusionSolarInverterThingClassId, name, desctiption) ;
                qCDebug(dcHuawei()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(huaweiFusionSolarInverterThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcHuawei()) << "This inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                ParamList params;
                params << Param(huaweiFusionSolarInverterThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(huaweiFusionSolarInverterThingSlaveIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();

    } else if (info->thingClassId() == huaweiRtuInverterThingClassId) {
        qCDebug(dcHuawei()) << "Discovering modbus RTU resources...";
        if (hardwareManager()->modbusRtuResource()->modbusRtuMasters().isEmpty()) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No Modbus RTU interface available. Please set up a Modbus RTU interface first."));
            return;
        }

        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), info);
        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with the required setting found (8 data bits, 1 stop bit, even parity)."));
                return;
            }

            qCInfo(dcHuawei()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(info->thingClassId(), "Huawei Inverter ", QString("Modbus ID: %1").arg(result.modbusId) + " " + result.serialPort);

                ParamList params{
                    {huaweiRtuInverterThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {huaweiRtuInverterThingModbusIdParamTypeId, result.modbusId}
                };
                descriptor.setParams(params);

                Thing *existingThing = myThings().findByParams(params);
                if (existingThing) {
                    descriptor.setThingId(existingThing->id());
                }
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
    }
}

void IntegrationPluginHuawei::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcHuawei()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == huaweiFusionSolarInverterThingClassId) {
        m_huaweiFusionSetupRunning = false;

        // Handle reconfigure
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcHuawei()) << "Reconfiguring existing thing" << thing->name();
            HuaweiFusionSolar *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_pvEnergyProducedValues.contains(thing))
            m_pvEnergyProducedValues.remove(thing);

        if (m_energyConsumedValues.contains(thing))
            m_energyConsumedValues.remove(thing);

        if (m_energyProducedValues.contains(thing))
            m_energyProducedValues.remove(thing);

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Test for null, because registering a monitor with null will cause a segfault. Mac is loaded from config, you can't be sure config contains a valid mac.
        MacAddress macAddress = MacAddress(thing->paramValue(huaweiFusionSolarInverterThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcHuawei()) << "Failed to set up Fusion Solar because the MAC address is not valid:" << thing->paramValue(huaweiFusionSolarInverterThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            qCDebug(dcHuawei()) << "Unregistering monitor because setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        });


        qCDebug(dcHuawei()) << "Monitor reachable" << monitor->reachable() << macAddress.toString();
        if (monitor->reachable()) {
            setupFusionSolar(info);
        } else {
            qCDebug(dcHuawei()) << "Network device" << thing->name() << "is not reachable yet. Continue with the setup once reachable.";
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                qCDebug(dcHuawei()) << "Monitor reachable changed!" << reachable;
                if (reachable && !m_huaweiFusionSetupRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_huaweiFusionSetupRunning = true;
                    setupFusionSolar(info);
                }
            });
        }
        return;
    }

    if (thing->thingClassId() == huaweiRtuInverterThingClassId) {

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcHuawei()) << "Reconfiguring existing thing" << thing->name();
            m_rtuConnections.take(thing)->deleteLater();
        }

        quint16 modbusId = thing->paramValue(huaweiRtuInverterThingModbusIdParamTypeId).toUInt();
        if (modbusId > 247 || modbusId == 0) {
            qCWarning(dcHuawei()) << "Setup failed, modbus ID is not valid" << modbusId;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The modbus ID is not valid. It must be a value between 1 and 247."));
            return;
        }

        ModbusRtuMaster *master = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(thing->paramValue(huaweiRtuInverterThingModbusMasterUuidParamTypeId).toUuid());
        if (!master) {
            qCWarning(dcHuawei()) << "The Modbus Master is not available any more.";
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The modbus RTU connection is not available."));
            return;
        }
        HuaweiModbusRtuConnection *connection = new HuaweiModbusRtuConnection(master, modbusId, thing);
        connect(info, &ThingSetupInfo::aborted, connection, [=](){
            qCDebug(dcHuawei()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
            connection->deleteLater();
        });

        connect(connection, &HuaweiModbusRtuConnection::reachableChanged, thing, [this, thing, connection](bool reachable){
            qCDebug(dcHuawei()) << thing->name() << "reachable changed" << reachable;
            thing->setStateValue(huaweiRtuInverterConnectedStateTypeId, reachable);

            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", reachable);
            }

            if (reachable) {
                qCDebug(dcHuawei()) << "Device" << thing << "is reachable via Modbus RTU on" << connection->modbusRtuMaster()->serialPort();

                // Reset history, just incase
                m_inverterEnergyProducedHistory[thing].clear();

                // Add the current value to the history
                checkEnergyValueReasonable(thing, thing->stateValue(huaweiRtuInverterTotalEnergyProducedStateTypeId).toFloat());
            } else {
                qCWarning(dcHuawei()) << "Device" << thing << "is not answering Modbus RTU calls on" << connection->modbusRtuMaster()->serialPort();
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                qCDebug(dcHuawei()) << "Huawei inverter initialized." << connection->model() << connection->serialNumber() << connection->productNumber();
                thing->setStateValue(huaweiRtuInverterModelStateTypeId, connection->model());
                thing->setStateValue(huaweiRtuInverterSerialNumberStateTypeId, connection->serialNumber());
                thing->setStateValue(huaweiRtuInverterProductNumberStateTypeId, connection->productNumber());
            } else {
                qCWarning(dcHuawei()) << "Huawei inverter initialization failed.";
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterInputPowerChanged, thing, [thing](float inverterInputPower){
            qCDebug(dcHuawei()) << "Inverter PV power changed" << inverterInputPower * -1000.0 << "W";
            thing->setStateValue(huaweiRtuInverterCurrentPowerStateTypeId, inverterInputPower * -1000.0);
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterDeviceStatusChanged, thing, [thing](HuaweiModbusRtuConnection::InverterDeviceStatus inverterDeviceStatus){
            qCDebug(dcHuawei()) << "Inverter device status changed" << inverterDeviceStatus;
            Q_UNUSED(thing)
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterEnergyProducedChanged, thing, [this, thing](float inverterEnergyProduced){
            qCDebug(dcHuawei()) << "Inverter total energy produced changed" << inverterEnergyProduced << "kWh";

            // Test value first, to avoid jitter.
            if (checkEnergyValueReasonable(thing, inverterEnergyProduced)) {
                thing->setStateValue(huaweiRtuInverterTotalEnergyProducedStateTypeId, inverterEnergyProduced);
            }
        });

        // Meter
        connect(connection, &HuaweiModbusRtuConnection::powerMeterActivePowerChanged, thing, [this, thing](qint32 powerMeterActivePower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter power changed" << powerMeterActivePower << "W";
                // Note: > 0 -> return, < 0 consume
                meterThings.first()->setStateValue(huaweiMeterCurrentPowerStateTypeId, -powerMeterActivePower);
            }
        });

        // Battery 1
        connect(connection, &HuaweiModbusRtuConnection::lunaBattery1StatusChanged, thing, [this, thing](HuaweiModbusRtuConnection::BatteryDeviceStatus lunaBattery1Status){
            qCDebug(dcHuawei()) << "Battery 1 status changed" << lunaBattery1Status;
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
                    qCDebug(dcHuawei()) << "Set up huawei energy storage 1 for" << thing;
                    ThingDescriptor descriptor(huaweiBatteryThingClassId, "Luna 2000 Battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(huaweiBatteryThingUnitParamTypeId, 1));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);
                }
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::lunaBattery1PowerChanged, thing, [this, thing](qint32 lunaBattery1Power){
            qCDebug(dcHuawei()) << "Battery 1 power changed" << lunaBattery1Power << "W";
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

        connect(connection, &HuaweiModbusRtuConnection::lunaBattery1SocChanged, thing, [this, thing](float lunaBattery1Soc){
            qCDebug(dcHuawei()) << "Battery 1 SOC changed" << lunaBattery1Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryBatteryLevelStateTypeId, lunaBattery1Soc);
                batteryThings.first()->setStateValue(huaweiBatteryBatteryCriticalStateTypeId, lunaBattery1Soc < 10);
            }
        });

        // Battery 2
        connect(connection, &HuaweiModbusRtuConnection::lunaBattery2StatusChanged, thing, [this, thing](HuaweiModbusRtuConnection::BatteryDeviceStatus lunaBattery1Status){
            qCDebug(dcHuawei()) << "Battery 2 status changed" << lunaBattery1Status;
            if (lunaBattery1Status != HuaweiModbusRtuConnection::BatteryDeviceStatusOffline) {
                // Check if w have to create the energy storage
                Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId);
                bool alreadySetUp = false;
                foreach (Thing *batteryThing, batteryThings) {
                    if (batteryThing->paramValue(huaweiBatteryThingUnitParamTypeId).toUInt() == 2) {
                        alreadySetUp = true;
                    }
                }

                if (!alreadySetUp) {
                    qCDebug(dcHuawei()) << "Set up huawei energy storage 2 for" << thing;
                    ThingDescriptor descriptor(huaweiBatteryThingClassId, "Luna 2000 Battery", QString(), thing->id());
                    ParamList params;
                    params.append(Param(huaweiBatteryThingUnitParamTypeId, 2));
                    descriptor.setParams(params);
                    emit autoThingsAppeared(ThingDescriptors() << descriptor);
                }
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::lunaBattery2PowerChanged, thing, [this, thing](qint32 lunaBattery2Power){
            qCDebug(dcHuawei()) << "Battery 2 power changed" << lunaBattery2Power << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryCurrentPowerStateTypeId, lunaBattery2Power);

                if (lunaBattery2Power < 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "discharging");
                } else if (lunaBattery2Power > 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::lunaBattery2SocChanged, thing, [this, thing](float lunaBattery2Soc){
            qCDebug(dcHuawei()) << "Battery 2 SOC changed" << lunaBattery2Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryBatteryLevelStateTypeId, lunaBattery2Soc);
                batteryThings.first()->setStateValue(huaweiBatteryBatteryCriticalStateTypeId, lunaBattery2Soc < 10);
            }
        });

        m_rtuConnections.insert(thing, connection);
        connection->initialize();

        // FIXME: make async and check if this is really a huawei
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

void IntegrationPluginHuawei::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == huaweiFusionSolarInverterThingClassId || thing->thingClassId() == huaweiRtuInverterThingClassId) {
        if (!m_pluginTimer) {
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(HuaweiFusionSolar *connection, m_tcpConnections) {
                    connection->update();
                }

                foreach(HuaweiModbusRtuConnection *connection, m_rtuConnections) {
                    // Use this register to test if initialization was done.
                    QString model = connection->model();
                    if (model.isEmpty()) {
                        connection->initialize();
                    } else {
                        connection->update();
                    }
                }
            });

            qCDebug(dcHuawei()) << "Starting plugin timer...";
            m_pluginTimer->start();
        }

        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId).isEmpty()) {
            qCDebug(dcHuawei()) << "Set up huawei meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(huaweiMeterThingClassId, "Huawei Power Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginHuawei::thingRemoved(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        HuaweiFusionSolar *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (m_pvEnergyProducedValues.contains(thing))
        m_pvEnergyProducedValues.remove(thing);

    if (m_energyConsumedValues.contains(thing))
        m_energyConsumedValues.remove(thing);

    if (m_energyProducedValues.contains(thing))
        m_energyProducedValues.remove(thing);

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

// Setup TCP connection
void IntegrationPluginHuawei::setupFusionSolar(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    uint port = thing->paramValue(huaweiFusionSolarInverterThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(huaweiFusionSolarInverterThingSlaveIdParamTypeId).toUInt();

    qCDebug(dcHuawei()) << "Setting up connection to fusion solar dongle" << monitor->networkDeviceInfo().address().toString() << port << modbusId;

    HuaweiFusionSolar *connection = new HuaweiFusionSolar(monitor->networkDeviceInfo().address(), port, modbusId, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcHuawei()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    connect(connection, &HuaweiFusionSolar::reachableChanged, info, [=](bool reachable){
        if (!reachable) {
            qCWarning(dcHuawei()) << "Connection init finished with errors" << thing->name() << connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->disconnectDevice();
            connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the SmartDongle."));
            return;
        }

        m_tcpConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);

        qCDebug(dcHuawei()) << "Setup huawei fusion solar smart dongle finished successfully" << monitor->networkDeviceInfo().address().toString() << port << modbusId;

        // Set connected state
        thing->setStateValue("connected", true);
        foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
            childThing->setStateValue("connected", true);
        }

        // Reset history, just incase
        m_inverterEnergyProducedHistory[thing].clear();

        // Add the current value to the history
        evaluateEnergyProducedValue(thing, thing->stateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId).toFloat());

        connect(connection, &HuaweiFusionSolar::reachableChanged, thing, [=](bool reachable){
            qCDebug(dcHuawei()) << "Reachable changed to" << reachable << "for" << thing;
            thing->setStateValue("connected", reachable);
            foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                childThing->setStateValue("connected", reachable);

                if (!reachable) {
                    // Set power values to 0 since we don't know what the current value is
                    if (childThing->thingClassId() == huaweiFusionSolarInverterThingClassId) {
                        thing->setStateValue(huaweiFusionSolarInverterCurrentPowerStateTypeId, 0);
                    }

                    if (childThing->thingClassId() == huaweiMeterThingClassId) {
                        thing->setStateValue(huaweiMeterCurrentPowerStateTypeId, 0);
                    }

                    if (childThing->thingClassId() == huaweiBatteryThingClassId) {
                        thing->setStateValue(huaweiBatteryCurrentPowerStateTypeId, 0);
                    }
                }
            }
        });

        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
            if (!thing->setupComplete())
                return;

            qCDebug(dcHuawei()) << "Network device monitor for" << thing->name() << (reachable ? "is now reachable" : "is not reachable any more" );

            if (reachable && !thing->stateValue("connected").toBool()) {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->connectDevice();
            } else if (!reachable) {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again
                connection->disconnectDevice();
            }
        });

        connect(connection, &HuaweiFusionSolar::inverterActivePowerChanged, thing, [thing](float inverterActivePower){
            thing->setStateValue(huaweiFusionSolarInverterActivePowerStateTypeId, inverterActivePower * -1000.0);
        });

        connect(connection, &HuaweiFusionSolar::inverterInputPowerChanged, thing, [thing](float inverterInputPower){
            thing->setStateValue(huaweiFusionSolarInverterCurrentPowerStateTypeId, inverterInputPower * -1000.0);
        });

        connect(connection, &HuaweiFusionSolar::inverterDeviceStatusReadFinished, thing, [thing](HuaweiFusionSolar::InverterDeviceStatus inverterDeviceStatus){
            qCDebug(dcHuawei()) << "Inverter device status changed" << inverterDeviceStatus;
            Q_UNUSED(thing)
        });

        connect(connection, &HuaweiFusionSolar::inverterEnergyProducedReadFinished, thing, [this, thing](float inverterEnergyProduced){
            qCDebug(dcHuawei()) << "Inverter total energy produced changed" << inverterEnergyProduced << "kWh";

            // Note: sometimes this value is suddenly 0 or absurd high > 100000000
            // We try here to filer out such random values. Sadly the values seem to
            // come like that from the device, without exception or error.
            evaluateEnergyProducedValue(thing, inverterEnergyProduced);
        });

        // Meter
        connect(connection, &HuaweiFusionSolar::powerMeterActivePowerReadFinished, thing, [this, thing](qint32 powerMeterActivePower){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter power changed" << powerMeterActivePower << "W";
                // Note: > 0 -> return, < 0 consume
                meterThings.first()->setStateValue(huaweiMeterCurrentPowerStateTypeId, -powerMeterActivePower);
            }
        });
        connect(connection, &HuaweiFusionSolar::powerMeterEnergyReturnedReadFinished, thing, [this, thing](float powerMeterEnergyReturned){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter power Returned changed" << powerMeterEnergyReturned << "kWh";
                meterThings.first()->setStateValue(huaweiMeterTotalEnergyProducedStateTypeId, powerMeterEnergyReturned);
            }
        });
        connect(connection, &HuaweiFusionSolar::powerMeterEnergyAquiredReadFinished, thing, [this, thing](float powerMeterEnergyAquired){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter power Aquired changed" << powerMeterEnergyAquired << "kWh";
                meterThings.first()->setStateValue(huaweiMeterTotalEnergyConsumedStateTypeId, powerMeterEnergyAquired);
            }
        });

        // Battery 1
        connect(connection, &HuaweiFusionSolar::lunaBattery1StatusReadFinished, thing, [this, thing](HuaweiFusionSolar::BatteryDeviceStatus lunaBattery1Status){
            qCDebug(dcHuawei()) << "Battery 1 status changed of" << thing << lunaBattery1Status;
            Thing *batteryThing = nullptr;
            foreach (Thing *bt, myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId)) {
                if (bt->paramValue(huaweiBatteryThingUnitParamTypeId).toUInt() == 1) {
                    batteryThing = bt;
                    break;
                }
            }

            // Check if w have to create the energy storage
            if (lunaBattery1Status != HuaweiFusionSolar::BatteryDeviceStatusOffline && !batteryThing) {
                qCDebug(dcHuawei()) << "Set up huawei energy storage 1 for" << thing;
                ThingDescriptor descriptor(huaweiBatteryThingClassId, "Luna 2000 Battery 1", QString(), thing->id());
                ParamList params;
                params.append(Param(huaweiBatteryThingUnitParamTypeId, 1));
                descriptor.setParams(params);
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            } else if (lunaBattery1Status == HuaweiFusionSolar::BatteryDeviceStatusOffline && batteryThing) {
                qCDebug(dcHuawei()) << "Autoremove huawei energy storage 1 for" << thing << "because the battery is offline" << batteryThing;
                emit autoThingDisappeared(batteryThing->id());
            }
        });

        connect(connection, &HuaweiFusionSolar::lunaBattery1PowerReadFinished, thing, [this, thing](qint32 lunaBattery1Power){
            qCDebug(dcHuawei()) << "Battery 1 power changed" << lunaBattery1Power << "W";
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

        connect(connection, &HuaweiFusionSolar::lunaBattery1SocReadFinished, thing, [this, thing](float lunaBattery1Soc){
            qCDebug(dcHuawei()) << "Battery 1 SOC changed" << lunaBattery1Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 1);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryBatteryLevelStateTypeId, lunaBattery1Soc);
                batteryThings.first()->setStateValue(huaweiBatteryBatteryCriticalStateTypeId, lunaBattery1Soc < 10);
            }
        });

        // Battery 2
        connect(connection, &HuaweiFusionSolar::lunaBattery2StatusReadFinished, thing, [this, thing](HuaweiFusionSolar::BatteryDeviceStatus lunaBattery2Status){

            qCDebug(dcHuawei()) << "Battery 2 status changed of" << thing << lunaBattery2Status;
            Thing *batteryThing = nullptr;
            foreach (Thing *bt, myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId)) {
                if (bt->paramValue(huaweiBatteryThingUnitParamTypeId).toUInt() == 2) {
                    batteryThing = bt;
                    break;
                }
            }

            // Check if w have to create the energy storage
            if (lunaBattery2Status != HuaweiFusionSolar::BatteryDeviceStatusOffline && !batteryThing) {
                qCDebug(dcHuawei()) << "Set up huawei energy storage 2 for" << thing;
                ThingDescriptor descriptor(huaweiBatteryThingClassId, "Luna 2000 Battery 2", QString(), thing->id());
                ParamList params;
                params.append(Param(huaweiBatteryThingUnitParamTypeId, 2));
                descriptor.setParams(params);
                emit autoThingsAppeared(ThingDescriptors() << descriptor);
            } else if (lunaBattery2Status == HuaweiFusionSolar::BatteryDeviceStatusOffline && batteryThing) {
                qCDebug(dcHuawei()) << "Autoremove huawei energy storage 2 for" << thing << "because the battery is offline" << batteryThing;
                emit autoThingDisappeared(batteryThing->id());
            }
        });

        connect(connection, &HuaweiFusionSolar::lunaBattery2PowerReadFinished, thing, [this, thing](qint32 lunaBattery2Power){
            qCDebug(dcHuawei()) << "Battery 2 power changed" << lunaBattery2Power << "W";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryCurrentPowerStateTypeId, lunaBattery2Power);

                if (lunaBattery2Power < 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "discharging");
                } else if (lunaBattery2Power > 0) {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "charging");
                } else {
                    batteryThings.first()->setStateValue(huaweiBatteryChargingStateStateTypeId, "idle");
                }
            }
        });

        connect(connection, &HuaweiFusionSolar::lunaBattery2SocReadFinished, thing, [this, thing](float lunaBattery2Soc){
            qCDebug(dcHuawei()) << "Battery 2 SOC changed" << lunaBattery2Soc << "%";
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiBatteryThingClassId).filterByParam(huaweiBatteryThingUnitParamTypeId, 2);
            if (!batteryThings.isEmpty()) {
                batteryThings.first()->setStateValue(huaweiBatteryBatteryLevelStateTypeId, lunaBattery2Soc);
                batteryThings.first()->setStateValue(huaweiBatteryBatteryCriticalStateTypeId, lunaBattery2Soc < 10);
            }
        });
    });

    if (monitor->reachable())
        connection->connectDevice();
}

// This function checks if a value seems legit (return true) or appears to be an error (return false) based on analysis of past values.
// The analysis is to look at the rate (change of one value to the next), and assume the rate should not change by more than x% (value of "percentLimit") over the timeframe of the
// analysed values, if they are legit.
bool IntegrationPluginHuawei::checkEnergyValueReasonable(Thing *inverterThing, float newValue)
{
    const int percentLimit = 10; // Change this if needed.

    // Add the value to the history
    m_inverterEnergyProducedHistory[inverterThing].append(newValue);

    int historyMaxSize = 3; // Don't change this.
    int historySize = m_inverterEnergyProducedHistory.value(inverterThing).count();

    if (historySize < historyMaxSize) {
        // Not enough values in history to do proper analysis. Declare current value as not legit (return false), since we don't know, and that is the save thing to do.
        return false;
    }

    if (historySize > historyMaxSize) {
        m_inverterEnergyProducedHistory[inverterThing].removeFirst();
    }

    // This code is specific to a historyMaxSize of 3.
    // How this works: Say the history has the following enties (oldest are first) [10, 11, 13].
    // "deltaLast" is 11 - 10 = 1
    // "deltaCurrent" is 13 - 11 = 2
    // "differenceInPercent" is then 100 * (2 - 1) / 1 = 100
    // This means the rate increased by 100%. Depending on the percentLimit variable, this is considered ok or not ok.
    float deltaLast = m_inverterEnergyProducedHistory.value(inverterThing).at(1) - m_inverterEnergyProducedHistory.value(inverterThing).at(0);
    float deltaCurrent = m_inverterEnergyProducedHistory.value(inverterThing).at(2) - m_inverterEnergyProducedHistory.value(inverterThing).at(1);
    float differenceInPercent = 100 * (deltaCurrent - deltaLast) / deltaLast;
    if ((differenceInPercent < -percentLimit) || (differenceInPercent > percentLimit)) {
        // Jitter detected.
        return false;
    }

    // Jitter test passed. The value is ok.
    return true;
}

void IntegrationPluginHuawei::evaluateEnergyProducedValue(Thing *inverterThing, float energyProduced)
{
    // Add the value to our small history
    m_inverterEnergyProducedHistory[inverterThing].append(energyProduced);

    int historyMaxSize  = 3;
    int absurdlyHighValueLimit= 25000000;
    int historySize = m_inverterEnergyProducedHistory.value(inverterThing).count();

    if (historySize > historyMaxSize) {
        m_inverterEnergyProducedHistory[inverterThing].removeFirst();
    }

    if (historySize == 1) {
        // First value add very high, add it to the history, but do not set this value until we get more absurde high value
        if (energyProduced > absurdlyHighValueLimit) {
            qCWarning(dcHuawei()) << "Energyfilter: First energy value absurdly high" << energyProduced << "...waiting for more values before accepting such values.";
            return;
        }

        // First value, no need to speculate here, we have no history
        inverterThing->setStateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId, energyProduced);
        return;
    }

    // Check if the new value is smaller than the current one, if so, we have either a counter reset or an invalid value we want to filter out
    float currentEnergyProduced = inverterThing->stateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId).toFloat();
    if (energyProduced < currentEnergyProduced) {
        // If more than 3 values arrive which are smaller than the current value,
        // we are sure this is a meter rest growing again and we assume they are correct
        // otherwise we just add the value to the history and wait for the next value before
        // we update the state

        if (historySize < historyMaxSize) {
            qCWarning(dcHuawei()) << "Energyfilter: Energy value" << energyProduced << "smaller than the last one. Still collecting history data" << m_inverterEnergyProducedHistory.value(inverterThing);
            return;
        }

        // Full history available, let's verify against the older values
        bool allValuesSmaller = true;
        for (int i = 0; i < historySize - 1; i++) {
            if (m_inverterEnergyProducedHistory.value(inverterThing).at(i) >= currentEnergyProduced) {
                allValuesSmaller = false;
                break;
            }
        }

        if (allValuesSmaller) {
            // We belive it, meter has been resetted
            qCDebug(dcHuawei()) << "Energyfilter: Energy value" << energyProduced << "seems to be really resetted back. Beliving it... History data:" << m_inverterEnergyProducedHistory.value(inverterThing);
            inverterThing->setStateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId, energyProduced);
        }

        return;
    } else if (energyProduced > absurdlyHighValueLimit) {
        // First value add very high, add it to the history, but do not set this value until we get more in this hight
        if (historySize < historyMaxSize) {
            qCWarning(dcHuawei()) << "Energyfilter: Energy value" << energyProduced << "absurdly high. Still collecting history data" << m_inverterEnergyProducedHistory.value(inverterThing);
            return;
        }

        // Full history available, let's verify against the older values
        bool allValuesAbsurdlyHigh = true;
        for (int i = 0; i < historySize - 1; i++) {
            if (m_inverterEnergyProducedHistory.value(inverterThing).at(i) < absurdlyHighValueLimit) {
                allValuesAbsurdlyHigh = false;
                break;
            }
        }

        if (allValuesAbsurdlyHigh) {
            // We belive it, they realy seem all absurdly high...
            qCDebug(dcHuawei()) << "Energyfilter: Energy value" << energyProduced << "seems to be really this absurdly high. Beliving it... History data:" << m_inverterEnergyProducedHistory.value(inverterThing);
            inverterThing->setStateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId, energyProduced);
        }
    } else {
        // Nothing strange detected, normal state update
        inverterThing->setStateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId, energyProduced);
    }
}

// This method uses the Hampel identifier (https://blogs.sas.com/content/iml/2021/06/01/hampel-filter-robust-outliers.html) to test if the value in the center of the window is an outlier or not.
// The input is a list of floats that contains the window of values to look at. The method will return true if the center value of that list is an outlier according to the Hampel
// identifier. If the value is not an outlier, the method will return false.
// The center value of the list is the one at (length / 2) for even length and ((length - 1) / 2) for odd length.
bool IntegrationPluginHuawei::isOutlier(const QList<float>& list)
{
    int const windowLength{list.length()};
    if (windowLength < 3) {
        qCWarning(dcHuawei()) << "Outlier check not working. Not enough values in the list.";
        return true;    // Unknown if the value is an outlier, but return true to not use the value because it can't be checked.
    }

    // This is the variable you can change to tweak outlier detection. It scales the size of the range in which values are deemed not an outlier. Increase the number to increase the
    // range (less values classified as an outlier), lower the number to reduce the range (more values classified as an outlier).
    uint const hampelH{3};

    float const madNormalizeFactor{1.4826};
    //qCDebug(dcHuawei()) << "Hampel identifier: the input list -" << list;
    QList<float> sortedList{list};
    std::sort(sortedList.begin(), sortedList.end());
    //qCDebug(dcHuawei()) << "Hampel identifier: the sorted list -" << sortedList;
    uint medianIndex;
    if (windowLength % 2 == 0) {
        medianIndex = windowLength / 2;
    } else {
        medianIndex = (windowLength - 1)/ 2;
    }
    float const median{sortedList.at(medianIndex)};
    //qCDebug(dcHuawei()) << "Hampel identifier: the median -" << median;

    QList<float> madList;
    for (int i = 0; i < windowLength; ++i) {
        madList.append(std::abs(median - sortedList.at(i)));
    }
    //qCDebug(dcHuawei()) << "Hampel identifier: the mad list -" << madList;

    std::sort(madList.begin(), madList.end());
    //qCDebug(dcHuawei()) << "Hampel identifier: the sorted mad list -" << madList;
    float const hampelIdentifier{hampelH * madNormalizeFactor * madList.at(medianIndex)};
    //qCDebug(dcHuawei()) << "Hampel identifier: the calculated Hampel identifier" << hampelIdentifier;

    bool isOutlier{std::abs(list.at(medianIndex) - median) > hampelIdentifier};
    //qCDebug(dcHuawei()) << "Hampel identifier: the value" << list.at(medianIndex) << " is an outlier?" << isOutlier;

    return isOutlier;
}


