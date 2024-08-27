/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright 2013 - 2021, nymea GmbH, Consolinno Energy GmbH, L. Heizinger
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

#include "integrationpluginbroetje.h"
#include "plugininfo.h"
#include "discoveryrtu.h"

IntegrationPluginBroetje::IntegrationPluginBroetje() {}

void IntegrationPluginBroetje::init() {
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcBgeTech()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(broetjeThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcBgeTech()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
                thing->setStateValue(broetjeConnectedStateTypeId, false);
                delete m_connections.take(thing);
            }
        }
    });
}

void IntegrationPluginBroetje::discoverThings(ThingDiscoveryInfo *info) {
    DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), info);
    connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
        if (!modbusMasterAvailable) {
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master found. Please set up a modbus RTU master first."));
            return;
        }

        qCInfo(dcBroetje()) << "Discovery results:" << discovery->discoveryResults().count();

        foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {
            ThingDescriptor descriptor(broetjeThingClassId, "Broetje", QString("Modbus ID: %1").arg(result.modbusId) + " on " + result.serialPort);

            ParamList params{
                {broetjeThingModbusIdParamTypeId, result.modbusId},
                {broetjeThingRtuMasterParamTypeId, result.modbusRtuMasterId}
            };
            descriptor.setParams(params);

            // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
            // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
            // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
            Thing *existingThing = myThings().findByParams(descriptor.params());
            if (existingThing) {
                qCDebug(dcBroetje()) << "Found already configured heat pump:" << existingThing->name();
                descriptor.setThingId(existingThing->id());
            } else {
                qCDebug(dcBroetje()) << "Found a Brötje heat pump with Modbus ID:" << result.modbusId << "on" << result.serialPort;
            }

            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
    });
    discovery->startDiscovery();
}

void IntegrationPluginBroetje::setupThing(ThingSetupInfo *info) {
    Thing *thing = info->thing();
    qCDebug(dcBroetje()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == broetjeThingClassId) {
        m_setupConnectionRunning = false;

        if (m_wpmConnections.contains(thing)) {
            qCDebug(dcBroetje()) << "Reconfiguring existing thing" << thing->name();
            WpmModbusTcpConnection *connection = m_wpmConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_lwzConnections.contains(thing)) {
            qCDebug(dcBroetje()) << "Reconfiguring existing thing" << thing->name();
            LwzModbusTcpConnection *connection = m_lwzConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Test for null, because registering a monitor with null will cause a segfault. Mac is loaded from config, you can't be sure config contains a valid mac.
        MacAddress macAddress = MacAddress(thing->paramValue(broetjeThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcBroetje()) << "Failed to set up Broetje heat pump because the MAC address is not valid:" << thing->paramValue(broetjeThingMacAddressParamTypeId).toString() << macAddress.toString();
            qCWarning(dcBroetje()) << "Please delete the device and add it again to fix this.";
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please delete the device and add it again to fix this."));
            return;
        }

        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcBroetje()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        qCDebug(dcBroetje()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(broetjeThingMacAddressParamTypeId).toString();
        if (monitor->reachable()) {
            setupConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info](bool reachable){
                qCDebug(dcBroetje()) << "Monitor reachable changed!" << reachable;
                if (reachable && !m_setupConnectionRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_setupConnectionRunning = true;
                    setupConnection(info);
                }
            });
        }
    }
}

void IntegrationPluginBroetje::postSetupThing(Thing *thing) {
    if (thing->thingClassId() == broetjeThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcBroetje()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (WpmModbusTcpConnection *connection, m_wpmConnections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
                }
                foreach (LwzModbusTcpConnection *connection, m_lwzConnections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginBroetje::thingRemoved(Thing *thing) {

    qCDebug(dcBroetje()) << "Delete thing" << thing->name();

    if (thing->thingClassId() == broetjeThingClassId && m_wpmConnections.contains(thing)) {
        WpmModbusTcpConnection *connection = m_wpmConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (thing->thingClassId() == broetjeThingClassId && m_lwzConnections.contains(thing)) {
        LwzModbusTcpConnection *connection = m_lwzConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    // Unregister related hardware resources
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginBroetje::executeAction(ThingActionInfo *info) {
    Thing *thing = info->thing();
    WpmModbusTcpConnection *connection = m_wpmConnections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcBroetje()) << "Could not execute action. The modbus connection is currently not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    // Got this from Broetje plugin, not sure if necessary
    if (thing->thingClassId() != broetjeThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }

    if (info->action().actionTypeId() == broetjeSgReadyActiveActionTypeId) {
        bool sgReadyActiveBool = info->action().paramValue(broetjeSgReadyActiveActionSgReadyActiveParamTypeId).toBool();
        qCDebug(dcBroetje()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        qCDebug(dcBroetje()) << "Value: " << sgReadyActiveBool;

        QModbusReply *reply = connection->setSgReadyActive(sgReadyActiveBool);
        if (!reply) {
            qCWarning(dcBroetje()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyActiveBool] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcBroetje()) << "Set SG ready activation finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcBroetje()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(broetjeSgReadyActiveStateTypeId, sgReadyActiveBool);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcBroetje()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    } else if (info->action().actionTypeId() == broetjeSgReadyModeActionTypeId) {
        QString sgReadyModeString = info->action().paramValue(broetjeSgReadyModeActionSgReadyModeParamTypeId).toString();
        qCDebug(dcBroetje()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        WpmModbusTcpConnection::SmartGridState sgReadyState;
        if (sgReadyModeString == "Off") {
            sgReadyState = WpmModbusTcpConnection::SmartGridStateModeOne;
        } else if (sgReadyModeString == "Low") {
            sgReadyState = WpmModbusTcpConnection::SmartGridStateModeTwo;
        } else if (sgReadyModeString == "Standard") {
            sgReadyState = WpmModbusTcpConnection::SmartGridStateModeThree;
        } else if (sgReadyModeString == "High") {
            sgReadyState = WpmModbusTcpConnection::SmartGridStateModeFour;
        } else {
            qCWarning(dcBroetje()) << "Failed to set SG Ready mode. An unknown SG Ready mode was passed: " << sgReadyModeString;
            info->finish(Thing::ThingErrorHardwareFailure);  // TODO better matching error type?
            return;
        }

        QModbusReply *reply = connection->setSgReadyState(sgReadyState);
        if (!reply) {
            qCWarning(dcBroetje()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyModeString] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcBroetje()) << "Set SG ready mode finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcBroetje()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(broetjeSgReadyModeStateTypeId, sgReadyModeString);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcBroetje()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    }
    info->finish(Thing::ThingErrorNoError);
}


void IntegrationPluginBroetje::setupConnection(ThingSetupInfo *info) {

    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    WpmModbusTcpConnection *connection = new WpmModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcBroetje()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcBroetje()) << "Monitor detected a change -" << thing << "is now reachable at" << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcBroetje()) << "Monitor detected a change -" << thing << "is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_wpmConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(broetjeConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcBroetje()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcBroetje()) << "The device is offline. Stopping communication attempts.";
                    m_wpmConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &WpmModbusTcpConnection::reachableChanged, thing, [this, thing, connection, monitor](bool reachable){
        qCDebug(dcBroetje()) << "Reachable changed to" << reachable << "for" << thing << ". Monitor is" << monitor->reachable();

        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(broetjeConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Broetje does not have any relevant states, like the current electric power.

            // Check the monitor. If the monitor is reachable, get the current IP (maybe it changed) and reconnect.
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_wpmConnections.contains(thing)) {
                    qCDebug(dcBroetje()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                }
            } else {
                // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                m_wpmConnections.value(thing)->disconnectDevice();
            }
        }
    });

    // This is needed because the monitor is sometimes too slow to trigger reachableChanged when the address changed.
    connect(monitor, &NetworkDeviceMonitor::networkDeviceInfoChanged, thing, [=](NetworkDeviceInfo networkDeviceInfo){

        if (monitor->reachable()) {
            qCDebug(dcBroetje()) << "Monitor detected a change -" << thing << "has changed it's network address and is now reachable at" << networkDeviceInfo;
        } else {
            qCDebug(dcBroetje()) << "Monitor detected a change -" << thing << "has changed it's network address and is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_wpmConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(broetjeConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (connection->reachable == false).
                if (monitor->reachable()) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcBroetje()) << "Connection is not working. Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcBroetje()) << "The device is offline. Stopping communication attempts.";
                    m_wpmConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &WpmModbusTcpConnection::initializationFinished, thing, [this, thing, connection, monitor](bool success){
        if (!thing->setupComplete())
            return;

        if (success) {
            uint controllerType = connection->controllerType();
            switch (controllerType) {
            case 390:
            case 391:
            case 449:
                qCDebug(dcBroetje()) << "Checking controller type:" << controllerType << " - ok.";
                thing->setStateValue(broetjeConnectedStateTypeId, true);
                break;
            default:
                qCWarning(dcBroetje()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                             << thing->paramValue(broetjeThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
                return;
            }
        } else {
            qCDebug(dcBroetje()) << "Initialization failed";
            thing->setStateValue(broetjeConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Broetje does not have any relevant states, like the current electric power.

            // Try once to reconnect the device
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_wpmConnections.contains(thing)) {
                    qCDebug(dcBroetje()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                }
            }
        }
    });

    connect(connection, &WpmModbusTcpConnection::initializationFinished, info, [=](bool success){
        if (!success) {
            qCWarning(dcBroetje()) << "Connection init finished with errors" << thing->name() << connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error communicating with the heat pump."));
            return;
        }

        qCDebug(dcBroetje()) << "Connection init finished successfully" << connection;

        uint controllerType = connection->controllerType();
        switch (controllerType) {
        case 390:
        case 391:
        case 449:
            qCDebug(dcBroetje()) << "Checking controller type:" << controllerType << " - ok.";
            break;
        default:
            qCWarning(dcBroetje()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                         << thing->paramValue(broetjeThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("Wrong configuration. Please reconfigure the device to fix this."));
            return;
        }

        m_wpmConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        thing->setStateValue(broetjeConnectedStateTypeId, true);
        connection->update();
    });


    connect(connection, &WpmModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
        qCDebug(dcBroetje()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
        thing->setStateValue(broetjeOutdoorTemperatureStateTypeId, outdoorTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::flowTemperatureChanged, thing, [this, thing](int flowTemperature){
        float convertedValue = dataType2conversion(flowTemperature);
        qCDebug(dcBroetje()) << thing << "flow temperature changed" << convertedValue << "°C";
        thing->setStateValue(broetjeFlowTemperatureStateTypeId, convertedValue);
    });

    connect(connection, &WpmModbusTcpConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
        qCDebug(dcBroetje()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
        thing->setStateValue(broetjeHotWaterTemperatureStateTypeId, hotWaterTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::storageTankTemperatureChanged, thing, [thing](float storageTankTemperature){
        qCDebug(dcBroetje()) << thing << "Storage tank temperature changed" << storageTankTemperature << "°C";
        thing->setStateValue(broetjeStorageTankTemperatureStateTypeId, storageTankTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::returnTemperatureChanged, thing, [thing](float returnTemperature){
        qCDebug(dcBroetje()) << thing << "return temperature changed" << returnTemperature << "°C";
        thing->setStateValue(broetjeReturnTemperatureStateTypeId, returnTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::heatingEnergyChanged, thing, [thing](quint32 heatingEnergy){
        // kWh and MWh of energy are stored in two registers an read as an uint32. The following arithmetic splits the uint32 into
        // two uint16 and sums up the MWh and kWh values.
        quint32 correctedEnergy = (heatingEnergy >> 16) + (heatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcBroetje()) << thing << "Heating energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(broetjeHeatingEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::hotWaterEnergyChanged, thing, [thing](quint32 hotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (hotWaterEnergy >> 16) + (hotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcBroetje()) << thing << "Hot Water energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(broetjeHotWaterEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::consumedEnergyHeatingChanged, thing, [thing](quint32 consumedEnergyHeatingEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHeatingEnergy >> 16) + (consumedEnergyHeatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcBroetje()) << thing << "Consumed energy Heating changed" << correctedEnergy << "kWh";
        thing->setStateValue(broetjeConsumedEnergyHeatingStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::consumedEnergyHotWaterChanged, thing, [thing](quint32 consumedEnergyHotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHotWaterEnergy >> 16) + (consumedEnergyHotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcBroetje()) << thing << "Consumed energy hot water changed" << correctedEnergy << "kWh";
        thing->setStateValue(broetjeConsumedEnergyHotWaterStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::operatingModeChanged, thing, [thing](WpmModbusTcpConnection::OperatingModeWpm operatingMode){
        qCDebug(dcBroetje()) << thing << "operating mode changed " << operatingMode;
        switch (operatingMode) {
        case WpmModbusTcpConnection::OperatingModeWpmEmergency:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Emergency");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmStandby:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Standby");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmProgram:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Program");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmComfort:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Comfort");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmEco:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Eco");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmHotWater:
            thing->setStateValue(broetjeOperatingModeStateTypeId, "Hot water");
            break;
        }
    });

    connect(connection, &WpmModbusTcpConnection::systemStatusChanged, thing, [thing](uint16_t systemStatus){
        qCDebug(dcBroetje()) << thing << "System status changed " << systemStatus;
        thing->setStateValue(broetjePumpOneStateTypeId, systemStatus & (1 << 0));
        thing->setStateValue(broetjePumpTwoStateTypeId, systemStatus & (1 << 1));
        thing->setStateValue(broetjeHeatingUpStateTypeId, systemStatus & (1 << 2));
        thing->setStateValue(broetjeAuxHeatingStateTypeId, systemStatus & (1 << 3));
        thing->setStateValue(broetjeHeatingStateTypeId, systemStatus & (1 << 4));
        thing->setStateValue(broetjeHotWaterStateTypeId, systemStatus & (1 << 5));
        thing->setStateValue(broetjeCompressorStateTypeId, systemStatus & (1 << 6));
        thing->setStateValue(broetjeSummerModeStateTypeId, systemStatus & (1 << 7));
        thing->setStateValue(broetjeCoolingModeStateTypeId, systemStatus & (1 << 8));
        thing->setStateValue(broetjeDefrostingStateTypeId, systemStatus & (1 << 9));
        thing->setStateValue(broetjeSilentModeStateTypeId, systemStatus & (1 << 10));
        thing->setStateValue(broetjeSilentMode2StateTypeId, systemStatus & (1 << 11));
    });

    connect(connection, &WpmModbusTcpConnection::faultStatusChanged, thing, [thing](uint16_t faultStatus){
        qCDebug(dcBroetje()) << thing << "Fault status changed " << faultStatus;
        thing->setStateValue(broetjeErrorStateTypeId, faultStatus);
    });

    connect(connection, &WpmModbusTcpConnection::sgReadyStateChanged, thing, [thing](WpmModbusTcpConnection::SmartGridState smartGridState){
        qCDebug(dcBroetje()) << thing << "SG Ready mode changed" << smartGridState;
        switch (smartGridState) {
        case WpmModbusTcpConnection::SmartGridStateModeOne:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Off");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeTwo:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Low");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeThree:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Standard");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeFour:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "High");
            break;
        }
    });

    connect(connection, &WpmModbusTcpConnection::sgReadyActiveChanged, thing, [thing](bool smartGridActive){
        qCDebug(dcBroetje()) << thing << "SG Ready activation changed" << smartGridActive;
        thing->setStateValue(broetjeSgReadyActiveStateTypeId, smartGridActive);
    });

    connect(connection, &WpmModbusTcpConnection::sgReadyStateROChanged, thing, [thing](uint16_t sgReadyStateRO){
        qCDebug(dcBroetje()) << thing << "SG Ready mode read changed" << sgReadyStateRO;
        switch (sgReadyStateRO) {
        case 1:
            thing->setStateValue(broetjeSgReadyReadStateTypeId, "Off");
            break;
        case 2:
            thing->setStateValue(broetjeSgReadyReadStateTypeId, "Low");
            break;
        case 3:
            thing->setStateValue(broetjeSgReadyReadStateTypeId, "Standard");
            break;
        case 4:
            thing->setStateValue(broetjeSgReadyReadStateTypeId, "High");
            break;
        }
    });

    connection->connectDevice();
}
