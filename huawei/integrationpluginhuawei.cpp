/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2024, nymea GmbH
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
#include "huaweismartloggerdiscovery.h"
#include "integrationpluginhuawei.h"
#include "plugininfo.h"

#include <hardware/modbus/modbusrtuhardwareresource.h>
#include <hardwaremanager.h>

IntegrationPluginHuawei::IntegrationPluginHuawei()
{

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

                QString description;

                if (!result.serialNumber.isEmpty())
                    description = "SN: " + result.serialNumber;

                switch (result.networkDeviceInfo.monitorMode()) {
                case NetworkDeviceInfo::MonitorModeMac:
                    description += " MAC: " + result.networkDeviceInfo.macAddressInfos().constFirst().macAddress().toString() +
                                   " - " + result.networkDeviceInfo.address().toString();
                    break;
                case NetworkDeviceInfo::MonitorModeHostName:
                    description += " Host name: " + result.networkDeviceInfo.hostName() +
                                   " - " + result.networkDeviceInfo.address().toString();
                    break;
                case NetworkDeviceInfo::MonitorModeIp:
                    description += " IP: " + result.networkDeviceInfo.address().toString();
                    break;
                }

                ThingDescriptor descriptor(huaweiFusionSolarInverterThingClassId, name, description) ;
                qCDebug(dcHuawei()) << "Discovered:" << descriptor.title() << descriptor.description();

                ParamList params;
                params << Param(huaweiFusionSolarInverterThingMacAddressParamTypeId, result.networkDeviceInfo.thingParamValueMacAddress());
                params << Param(huaweiFusionSolarInverterThingHostNameParamTypeId, result.networkDeviceInfo.thingParamValueHostName());
                params << Param(huaweiFusionSolarInverterThingAddressParamTypeId, result.networkDeviceInfo.thingParamValueAddress());
                params << Param(huaweiFusionSolarInverterThingSlaveIdParamTypeId, result.slaveId);
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(params);
                if (existingThing) {
                    qCDebug(dcHuawei()) << "This inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();

    } else if (info->thingClassId() == huaweiSmartLoggerThingClassId) {

        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcHuawei()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        HuaweiSmartLoggerDiscovery *discovery = new HuaweiSmartLoggerDiscovery(hardwareManager()->networkDeviceDiscovery(), 502, info);
        connect(discovery, &HuaweiSmartLoggerDiscovery::discoveryFinished, info, [this, info, discovery](){
            foreach (const HuaweiSmartLoggerDiscovery::Result &result, discovery->results()) {

                QString name = QT_TR_NOOP("Huawei SmartLogger");

                QString description;
                switch (result.networkDeviceInfo.monitorMode()) {
                case NetworkDeviceInfo::MonitorModeMac:
                    description += " MAC: " + result.networkDeviceInfo.macAddressInfos().constFirst().macAddress().toString() +
                                   " - " + result.networkDeviceInfo.address().toString();
                    break;
                case NetworkDeviceInfo::MonitorModeHostName:
                    description += " Host name: " + result.networkDeviceInfo.hostName() +
                                   " - " + result.networkDeviceInfo.address().toString();
                    break;
                case NetworkDeviceInfo::MonitorModeIp:
                    description += " IP: " + result.networkDeviceInfo.address().toString();
                    break;
                }

                ThingDescriptor descriptor(huaweiSmartLoggerThingClassId, name, description) ;
                qCDebug(dcHuawei()) << "Discovered:" << descriptor.title() << descriptor.description();

                ParamList params;
                params << Param(huaweiSmartLoggerThingMacAddressParamTypeId, result.networkDeviceInfo.thingParamValueMacAddress());
                params << Param(huaweiSmartLoggerThingHostNameParamTypeId, result.networkDeviceInfo.thingParamValueHostName());
                params << Param(huaweiSmartLoggerThingAddressParamTypeId, result.networkDeviceInfo.thingParamValueAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Thing *existingThing = myThings().findByParams(params);
                if (existingThing) {
                    qCDebug(dcHuawei()) << "This smartlogger already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThing->id());
                }

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

        uint slaveAddress = info->params().paramValue(huaweiRtuInverterThingSlaveAddressParamTypeId).toUInt();
        if (slaveAddress > 254 || slaveAddress == 0) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The Modbus slave address must be a value between 1 and 254."));
            return;
        }

        foreach (ModbusRtuMaster *modbusMaster, hardwareManager()->modbusRtuResource()->modbusRtuMasters()) {
            qCDebug(dcHuawei()) << "Found RTU master resource" << modbusMaster << "connected" << modbusMaster->connected();
            if (!modbusMaster->connected())
                continue;

            ThingDescriptor descriptor(info->thingClassId(), "Huawei Inverter", QString::number(slaveAddress) + " " + modbusMaster->serialPort());
            ParamList params;
            params << Param(huaweiRtuInverterThingSlaveAddressParamTypeId, slaveAddress);
            params << Param(huaweiRtuInverterThingModbusMasterUuidParamTypeId, modbusMaster->modbusUuid());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginHuawei::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcHuawei()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == huaweiFusionSolarInverterThingClassId) {

        // Handle reconfigure
        if (m_connections.contains(thing))
            m_connections.take(thing)->deleteLater();

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(thing);
        if (!monitor) {
            qCWarning(dcHuawei()) << "Failed to set up Fusion Solar because the params are incomplete for creating a monitor:" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The parameters are incomplete. Please reconfigure the device to fix this."));
            return;
        }

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
                    if (reachable) {
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

    if (thing->thingClassId() == huaweiSmartLoggerThingClassId) {

        // Handle reconfigure
        if (m_smartLoggerConnections.contains(thing))
            m_smartLoggerConnections.take(thing)->deleteLater();

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(thing);
        if (!monitor) {
            qCWarning(dcHuawei()) << "Failed to set up SmartLogger because the params are incomplete for creating a monitor:" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The parameters are incomplete. Please reconfigure the device to fix this."));
            return;
        }

        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        });

        // Continue with setup only if we know that the network device is reachable
        if (info->isInitialSetup()) {
            if (monitor->reachable()) {
                setupSmartLogger(info);
            } else {
                // otherwise wait until we reach the networkdevice before setting up the device
                qCDebug(dcHuawei()) << "Network device" << thing->name() << "is not reachable yet. Continue with the setup once reachable.";
                connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                    if (reachable) {
                        qCDebug(dcHuawei()) << "Network device" << thing->name() << "is now reachable. Continue with the setup...";
                        setupSmartLogger(info);
                    }
                });
            }
        } else {
            setupSmartLogger(info);
        }

        return;
    }

    if (thing->thingClassId() == huaweiRtuInverterThingClassId) {

        uint address = thing->paramValue(huaweiRtuInverterThingSlaveAddressParamTypeId).toUInt();
        if (address > 254 || address == 0) {
            qCWarning(dcHuawei()) << "Setup failed, slave address is not valid" << address;
            info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The Modbus address not valid. It must be a value between 1 and 254."));
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

        ModbusRtuMaster *rtuMaster = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(uuid);
        HuaweiModbusRtuConnection *connection = new HuaweiModbusRtuConnection(rtuMaster, address, this);

        connect(connection, &HuaweiModbusRtuConnection::reachableChanged, thing, [this, thing, connection](bool reachable){
            qCDebug(dcHuawei()) << thing->name() << "reachable changed" << reachable;
            if (reachable) {
                // Connected true will be set after successfull init
                connection->initialize();
            } else {
                thing->setStateValue("connected", false);
                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", false);
                }
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::initializationFinished, thing, [this, thing, connection](bool success){
            if (success) {
                thing->setStateValue("connected", true);
                foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
                    childThing->setStateValue("connected", true);
                }

                connection->update();
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterActivePowerChanged, thing, [thing](float inverterActivePower){
            qCDebug(dcHuawei()) << "Inverter power changed" << inverterActivePower * -1000.0 << "W";
            thing->setStateValue(huaweiRtuInverterCurrentPowerStateTypeId, inverterActivePower * -1000.0);
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterDeviceStatusChanged, thing, [thing](HuaweiModbusRtuConnection::InverterDeviceStatus inverterDeviceStatus){
            qCDebug(dcHuawei()) << "Inverter device status changed" << inverterDeviceStatus;
            Q_UNUSED(thing)
        });

        connect(connection, &HuaweiModbusRtuConnection::inverterEnergyProducedChanged, thing, [thing](float inverterEnergyProduced){
            qCDebug(dcHuawei()) << "Inverter total energy produced changed" << inverterEnergyProduced << "kWh";
            thing->setStateValue(huaweiRtuInverterTotalEnergyProducedStateTypeId, inverterEnergyProduced);
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

        connect(connection, &HuaweiModbusRtuConnection::powerMeterEnergyReturnedChanged, thing, [this, thing](float powerMeterEnergyReturned){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter Total Energy Returned changed" << powerMeterEnergyReturned << "KWh";
                meterThings.first()->setStateValue(huaweiMeterTotalEnergyProducedStateTypeId, powerMeterEnergyReturned);
            }
        });

        connect(connection, &HuaweiModbusRtuConnection::powerMeterEnergyAquiredChanged, thing, [this, thing](float powerMeterEnergyAquired){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId);
            if (!meterThings.isEmpty()) {
                qCDebug(dcHuawei()) << "Meter power Energy Aquired changed" << powerMeterEnergyAquired << "KWh";
                meterThings.first()->setStateValue(huaweiMeterTotalEnergyConsumedStateTypeId, powerMeterEnergyAquired);
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
    if (thing->thingClassId() == huaweiFusionSolarInverterThingClassId ||
        thing->thingClassId() == huaweiRtuInverterThingClassId ||
        thing->thingClassId() == huaweiSmartLoggerThingClassId) {

        if (!m_pluginTimer) {
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach(HuaweiFusionSolar *connection, m_connections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
                }

                foreach(HuaweiSmartLoggerModbusTcpConnection *connection, m_smartLoggerConnections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
                }

                foreach(HuaweiModbusRtuConnection *connection, m_rtuConnections) {
                    connection->update();
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

    if (m_smartLoggerConnections.contains(thing)) {
        HuaweiSmartLogger *connection = m_smartLoggerConnections.take(thing);
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
    connect(info, &ThingSetupInfo::aborted, connection, [this, connection, thing](){
        connection->deleteLater();
        m_connections.remove(thing);
    });

    m_connections.insert(thing, connection);
    info->finish(Thing::ThingErrorNoError);

    qCDebug(dcHuawei()) << "Setup huawei fusion solar smart dongle finished successfully" << monitor->networkDeviceInfo().address().toString() << port << slaveId;

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
            connection->modbusTcpMaster()->setHostAddress(monitor->networkDeviceInfo().address());
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

    connection->connectDevice();
}

void IntegrationPluginHuawei::setupSmartLogger(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    uint port = thing->paramValue(huaweiSmartLoggerThingPortParamTypeId).toUInt();
    quint16 meterSlaveId = thing->paramValue(huaweiSmartLoggerThingMeterSlaveIdParamTypeId).toUInt();

    qCDebug(dcHuawei()) << "Setup connection to smarlogger on" << monitor->networkDeviceInfo().address().toString() << port << "Meter slave ID" << meterSlaveId;

    HuaweiSmartLogger *connection = new HuaweiSmartLogger(monitor->networkDeviceInfo().address(), port, meterSlaveId, this);
    connect(info, &ThingSetupInfo::aborted, connection, [this, connection, thing](){
        connection->deleteLater();
        m_smartLoggerConnections.remove(thing);
    });

    m_smartLoggerConnections.insert(thing, connection);
    info->finish(Thing::ThingErrorNoError);

    qCDebug(dcHuawei()) << "Setup huawei smart logger finished successfully";

    // Reset history, just incase
    m_inverterEnergyProducedHistory[thing].clear();

    // Add the current value to the history
    evaluateEnergyProducedValue(thing, thing->stateValue(huaweiSmartLoggerTotalEnergyProducedStateTypeId).toFloat());

    connect(connection, &HuaweiSmartLogger::reachableChanged, thing, [=](bool reachable){
        qCDebug(dcHuawei()) << "Reachable changed to" << reachable << "for" << thing;
        thing->setStateValue("connected", reachable);
        foreach (Thing *childThing, myThings().filterByParentId(thing->id())) {
            childThing->setStateValue("connected", reachable);

            if (!reachable) {
                // Set power values to 0 since we don't know what the current value is
                if (childThing->thingClassId() == huaweiSmartLoggerThingClassId) {
                    thing->setStateValue(huaweiSmartLoggerCurrentPowerStateTypeId, 0);
                }

                if (childThing->thingClassId() == huaweiMeterThingClassId) {
                    thing->setStateValue(huaweiMeterCurrentPowerStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPhaseAStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPhaseBStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPhaseCStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPowerPhaseAStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPowerPhaseBStateTypeId, 0);
                    thing->setStateValue(huaweiMeterCurrentPowerPhaseCStateTypeId, 0);
                    thing->setStateValue(huaweiMeterVoltagePhaseAStateTypeId, 0);
                    thing->setStateValue(huaweiMeterVoltagePhaseBStateTypeId, 0);
                    thing->setStateValue(huaweiMeterVoltagePhaseCStateTypeId, 0);
                }
            }
        }
    });

    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable){
        if (!thing->setupComplete())
            return;

        qCDebug(dcHuawei()) << "Network device monitor for" << thing->name() << (reachable ? "is now reachable" : "is not reachable any more" );

        if (reachable && !thing->stateValue("connected").toBool()) {
            connection->modbusTcpMaster()->setHostAddress(monitor->networkDeviceInfo().address());
            connection->connectDevice();
        } else if (!reachable) {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &HuaweiSmartLoggerModbusTcpConnection::updateFinished, thing, [this, thing, connection](){
        qCDebug(dcHuawei()) << "Smartlogger update finished" << thing << qobject_cast<HuaweiSmartLoggerModbusTcpConnection *>(connection);

        // Set inverter data
        thing->setStateValue(huaweiSmartLoggerCurrentPowerStateTypeId, connection->inverterTotalActivePower() * -1);
        thing->setStateValue(huaweiSmartLoggerTotalEnergyProducedStateTypeId, connection->inverterTotalEnergyProduced());

        // Update the meter data
        Thing *meterThing = myThings().filterByParentId(thing->id()).filterByThingClassId(huaweiMeterThingClassId).first();
        if (!meterThing)
            return;

        meterThing->setStateValue(huaweiMeterCurrentPowerStateTypeId, connection->meterActivePower());
        meterThing->setStateValue(huaweiMeterCurrentPhaseAStateTypeId, connection->meterCurrentPhaseA() * -1);
        meterThing->setStateValue(huaweiMeterCurrentPhaseBStateTypeId, connection->meterCurrentPhaseB()* -1);
        meterThing->setStateValue(huaweiMeterCurrentPhaseCStateTypeId, connection->meterCurrentPhaseC()* -1);
        meterThing->setStateValue(huaweiMeterCurrentPowerPhaseAStateTypeId, connection->meterPowerPhaseA());
        meterThing->setStateValue(huaweiMeterCurrentPowerPhaseBStateTypeId, connection->meterPowerPhaseB());
        meterThing->setStateValue(huaweiMeterCurrentPowerPhaseCStateTypeId, connection->meterPowerPhaseC());
        meterThing->setStateValue(huaweiMeterVoltagePhaseAStateTypeId, connection->meterVoltagePhaseA());
        meterThing->setStateValue(huaweiMeterVoltagePhaseBStateTypeId, connection->meterVoltagePhaseB());
        meterThing->setStateValue(huaweiMeterVoltagePhaseCStateTypeId, connection->meterVoltagePhaseC());

        meterThing->setStateValue(huaweiMeterTotalEnergyProducedStateTypeId, connection->meterNegativeActiveElectricity());
        meterThing->setStateValue(huaweiMeterTotalEnergyConsumedStateTypeId, connection->meterPositiveActiveElectricity());
    });

    connection->connectDevice();
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

