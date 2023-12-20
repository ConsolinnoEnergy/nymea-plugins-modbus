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
            if (thing->paramValue(solaxX3InverterRTUThingModbusMasterUuidParamTypeId) == modbusUuid) {
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
        QList<quint16> slaveIds = {1, 2, 3};
        HuaweiFusionSolarDiscovery *discovery = new HuaweiFusionSolarDiscovery(hardwareManager()->networkDeviceDiscovery(), 502, slaveIds, info);
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
                params << Param(huaweiFusionSolarInverterThingSlaveIdParamTypeId, result.slaveId);
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
                    {solaxX3InverterRTUThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId},
                    {solaxX3InverterRTUThingModbusIdParamTypeId, result.modbusId}
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
        if (m_connections.contains(thing))
            m_connections.take(thing)->deleteLater();

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Make sure we have a valid mac address, otherwise no monitor and not auto searching is possible
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
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        });


        // Continue with setup only if we know that the network device is reachable
        if (info->isInitialSetup()) {
            if (monitor->reachable()) {
                setupFusionSolar(info);
            } else {
                // otherwise wait until we reach the networkdevice before setting up the device
                qCDebug(dcHuawei()) << "Network device" << thing->name() << "is not reachable yet. Continue with the setup once reachable.";
                connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                    if (reachable && !m_huaweiFusionSetupRunning) {
                        m_huaweiFusionSetupRunning = true;
                        qCDebug(dcHuawei()) << "Network device" << thing->name() << "is now reachable. Continue with the setup...";
                        setupFusionSolar(info);
                    }
                });
            }
        } else {
            setupFusionSolar(info);
        }

        return;
    }

    if (thing->thingClassId() == huaweiRtuInverterThingClassId) {

        uint address = thing->paramValue(huaweiRtuInverterThingSlaveAddressParamTypeId).toUInt();
        if (address > 247 || address == 0) {
            qCWarning(dcHuawei()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 247."));
            return;
        }

        QUuid uuid = thing->paramValue(huaweiRtuInverterThingModbusMasterUuidParamTypeId).toUuid();
        if (!hardwareManager()->modbusRtuResource()->hasModbusRtuMaster(uuid)) {
            qCWarning(dcHuawei()) << "Setup failed, hardware manager not available";
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus RTU resource is not available."));
            return;
        }

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcHuawei()) << "Already have a Huawei connection for this thing. Cleaning up old connection and initializing new one...";
            delete m_rtuConnections.take(thing);
        }


        HuaweiModbusRtuConnection *connection = new HuaweiModbusRtuConnection(hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid), address, this);
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
                checkEnergyValueReasonable(thing, thing->stateValue(huaweiRtuInverterTotalEnergyProducedStateTypeId).toFloat(), 0.0);
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

        connect(connection, &HuaweiModbusRtuConnection::inverterEnergyProducedChanged, thing, [thing](float inverterEnergyProduced){
            qCDebug(dcHuawei()) << "Inverter total energy produced changed" << inverterEnergyProduced << "kWh";

            // Test value first, to avoid jitter.
            if (checkEnergyValueReasonable(thing, inverterEnergyProduced, thing->stateValue(huaweiFusionSolarInverterTotalEnergyProducedStateTypeId).toFloat())) {
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
                foreach(HuaweiFusionSolar *connection, m_connections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
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

    if (m_connections.contains(thing)) {
        HuaweiFusionSolar *connection = m_connections.take(thing);
        connection->disconnectDevice();
        delete connection;
    }

    if (m_rtuConnections.contains(thing)) {
        m_rtuConnections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginHuawei::setupFusionSolar(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    uint port = thing->paramValue(huaweiFusionSolarInverterThingPortParamTypeId).toUInt();
    quint16 slaveId = thing->paramValue(huaweiFusionSolarInverterThingSlaveIdParamTypeId).toUInt();

    qCDebug(dcHuawei()) << "Setup connection to fusion solar dongle" << monitor->networkDeviceInfo().address().toString() << port << slaveId;

    HuaweiFusionSolar *connection = new HuaweiFusionSolar(monitor->networkDeviceInfo().address(), port, slaveId, this);
    connect(info, &ThingSetupInfo::aborted, connection, &HuaweiFusionSolar::deleteLater);
    connect(connection, &HuaweiFusionSolar::reachableChanged, info, [=](bool reachable){
        if (!reachable) {
            qCWarning(dcHuawei()) << "Connection init finished with errors" << thing->name() << connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->disconnectDevice();
            connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the SmartDongle."));
            return;
        }

        m_connections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);

        qCDebug(dcHuawei()) << "Setup huawei fusion solar smart dongle finished successfully" << monitor->networkDeviceInfo().address().toString() << port << slaveId;

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

// This function checks if a value seems legit (return true) or is significantly different to the previous values that it seems
// to be an error and should be discarded (return false).
bool IntegrationPluginHuawei::checkEnergyValueReasonable(Thing *inverterThing, float newValue, float lastLegitValue)
{
    // Add the value to our small history
    m_inverterEnergyProducedHistory[inverterThing].append(newValue);

    int historyMaxSize  = 3;
    int historySize = m_inverterEnergyProducedHistory.value(inverterThing).count();

    if (historySize > historyMaxSize) {
        m_inverterEnergyProducedHistory[inverterThing].removeFirst();
    }

    if (historySize == 1) {
        // When this function is called for the first time, the sole purpose is to put the default or cached value of the state in the history.
        // Since that values is already in the state, no need to decide if the value should be put in the state or not. Hence, the return value
        // is not used and it does not matter what is returned.
        return true;
    }

    // Check if the new value is smaller than the current one. If yes, we have either a counter reset or an invalid value we want to filter out.
    // Test if this is a counter reset. If not, discard the value.
    // The test for a counter reset is:
    // - lastLegitValue is not in the history anymore. When a counter reset occurs, all new values will be smaller than lastLegitValue and hence discaded.
    // The history will fill up with discarded values until lastLegitValue is pushed out.
    // - all the values in the history are monotonically increasing and the difference between them will be small (<5%).
    if (newValue < lastLegitValue) {

        // Not enough values in history, can't evaluate. Discard this value, just to be save.
        if (historySize < historyMaxSize) {
            qCWarning(dcHuawei()) << "Energyfilter: Energy value" << newValue << "is smaller than the last one. Still collecting history data" << m_inverterEnergyProducedHistory.value(inverterThing);
            return false;
        }

        // Test if lastLegitValue has been pushed out of history yet. Test every value but the last one, because the last one is newValue.
        for (int i = 0; i < historySize - 1; i++) {
            // Comparing floats is tricky. Need to allow for some leeway.
            if (m_inverterEnergyProducedHistory.value(inverterThing).at(i) <= (lastLegitValue * 1,0001)
                    || m_inverterEnergyProducedHistory.value(inverterThing).at(i) >= (lastLegitValue * 0,9999)) {

                // lastLegitValue (or a value very similar to it) is still in the history. Threshold for counter reset not reached yet. Discard this value.
                return false;
            }
        }

        // Only discarded values are in the history. Proceed with "monotonically increasing with small steps" test.
    }

    // Test if the values in the history are monotonically increasing and the difference between them is small (<5%).
    for (int i = 0; i < historySize - 1; i++) {
        float thisValue = m_inverterEnergyProducedHistory.value(inverterThing).at(i);
        float nextValue = m_inverterEnergyProducedHistory.value(inverterThing).at(i + 1);

        // Comparing floats is tricky. Make nextValue a tiny bit bigger, to make sure this if is not true when thisValue and nextValue are nearly identical.
        if (thisValue > (nextValue * 1,0001)) {
            // Not monotonically increasing.
            qCWarning(dcHuawei()) << "Jitter detected in recently transmitted values" << m_inverterEnergyProducedHistory.value(inverterThing) << ", discarding values until jitter stops.";
            return false;
        }

        float increaseInPercent = 100 * (nextValue - thisValue) / thisValue;
        if (increaseInPercent > 5) {
            // Difference is too large. Incoming values jitter too much.
            qCWarning(dcHuawei()) << "Jitter detected in recently transmitted values" << m_inverterEnergyProducedHistory.value(inverterThing) << ", discarding values until jitter stops.";
            return false;
        }
    }

    // All tests sucessful, the value is legit.
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
