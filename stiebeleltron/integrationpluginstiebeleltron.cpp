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

#include "integrationpluginstiebeleltron.h"
#include "plugininfo.h"
#include "discovery.h"

#include <hardwaremanager.h>
#include <network/networkdevicediscovery.h>

IntegrationPluginStiebelEltron::IntegrationPluginStiebelEltron() {}

void IntegrationPluginStiebelEltron::discoverThings(ThingDiscoveryInfo *info) {
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcStiebelEltron()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature,
                     QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    Discovery *discovery = new Discovery(hardwareManager()->networkDeviceDiscovery(), info);
    connect(discovery, &Discovery::discoveryFinished, info, [this, info, discovery](){
        qCInfo(dcStiebelEltron()) << "Discovery results:" << discovery->discoveryResults().count();

        foreach (const Discovery::Result &result, discovery->discoveryResults()) {
            ThingDescriptor descriptor(stiebelEltronThingClassId, "Stiebel Eltron", QString("MAC: %1").arg(result.networkDeviceInfo.macAddress()));

            ParamList params{
                {stiebelEltronThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress()},
                {stiebelEltronThingControllerTypeParamTypeId, result.controllerType}
            };
            descriptor.setParams(params);

            // Check if we already have set up this device. Only check the MAC and not the controllerType, to allow a reconfigure to change controllerType.
            Things existingThings = myThings().filterByParam(stiebelEltronThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
            if (existingThings.count() >= 1) {
                qCDebug(dcStiebelEltron()) << "This connection already exists in the system:" << result.networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);

    });
    discovery->startDiscovery();
}

void IntegrationPluginStiebelEltron::setupThing(ThingSetupInfo *info) {
    Thing *thing = info->thing();
    qCDebug(dcStiebelEltron()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == stiebelEltronThingClassId) {
        m_setupConnectionRunning = false;

        if (m_wpmConnections.contains(thing)) {
            qCDebug(dcStiebelEltron()) << "Reconfiguring existing thing" << thing->name();
            WpmModbusTcpConnection *connection = m_wpmConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_lwzConnections.contains(thing)) {
            qCDebug(dcStiebelEltron()) << "Reconfiguring existing thing" << thing->name();
            LwzModbusTcpConnection *connection = m_lwzConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Test for null, because registering a monitor with null will cause a segfault. Mac is loaded from config, you can't be sure config contains a valid mac.
        MacAddress macAddress = MacAddress(thing->paramValue(stiebelEltronThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcStiebelEltron()) << "Failed to set up Stiebel Eltron heat pump because the MAC address is not valid:" << thing->paramValue(stiebelEltronThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcStiebelEltron()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        qCDebug(dcStiebelEltron()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(stiebelEltronThingMacAddressParamTypeId).toString();
        if (monitor->reachable()) {
            setupConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info](bool reachable){
                qCDebug(dcStiebelEltron()) << "Monitor reachable changed!" << reachable;
                if (reachable && !m_setupConnectionRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_setupConnectionRunning = true;
                    setupConnection(info);
                }
            });
        }
    }
}

void IntegrationPluginStiebelEltron::postSetupThing(Thing *thing) {
    if (thing->thingClassId() == stiebelEltronThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcStiebelEltron()) << "Starting plugin timer...";
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

void IntegrationPluginStiebelEltron::thingRemoved(Thing *thing) {

    qCDebug(dcStiebelEltron()) << "Delete thing" << thing->name();

    if (thing->thingClassId() == stiebelEltronThingClassId && m_wpmConnections.contains(thing)) {
        WpmModbusTcpConnection *connection = m_wpmConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (thing->thingClassId() == stiebelEltronThingClassId && m_lwzConnections.contains(thing)) {
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

void IntegrationPluginStiebelEltron::executeAction(ThingActionInfo *info) {
    Thing *thing = info->thing();
    uint controllerType = thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt();
    switch (controllerType) {
    case 103:
    case 104:
        executeActionLwz(info);
        break;
    case 390:
    case 391:
    case 449:
        executeActionWpm(info);
        break;
    default:
        qCWarning(dcStiebelEltron()) << "Failed to execute action for Stiebel Eltron heat pump because of invalid config parameter: controller type" << controllerType << ". Please reconfigure the device to fix this.";
        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The controller type is not vaild. Please reconfigure the device to fix this."));
        break;
    }
}

void IntegrationPluginStiebelEltron::executeActionWpm(ThingActionInfo *info) {
    Thing *thing = info->thing();
    WpmModbusTcpConnection *connection = m_wpmConnections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcStiebelEltron()) << "Could not execute action. The modbus connection is currently not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    // Got this from StiebelEltron plugin, not sure if necessary
    if (thing->thingClassId() != stiebelEltronThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }

    if (info->action().actionTypeId() == stiebelEltronSgReadyActiveActionTypeId) {
        bool sgReadyActiveBool = info->action().paramValue(stiebelEltronSgReadyActiveActionSgReadyActiveParamTypeId).toBool();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        qCDebug(dcStiebelEltron()) << "Value: " << sgReadyActiveBool;

        QModbusReply *reply = connection->setSgReadyActive(sgReadyActiveBool);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyActiveBool] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron()) << "Set SG ready activation finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyActiveStateTypeId, sgReadyActiveBool);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    } else if (info->action().actionTypeId() == stiebelEltronSgReadyModeActionTypeId) {
        QString sgReadyModeString = info->action().paramValue(stiebelEltronSgReadyModeActionSgReadyModeParamTypeId).toString();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
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
            qCWarning(dcStiebelEltron()) << "Failed to set SG Ready mode. An unknown SG Ready mode was passed: " << sgReadyModeString;
            info->finish(Thing::ThingErrorHardwareFailure);  // TODO better matching error type?
            return;
        }

        QModbusReply *reply = connection->setSgReadyState(sgReadyState);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyModeString] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron()) << "Set SG ready mode finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyModeStateTypeId, sgReadyModeString);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginStiebelEltron::executeActionLwz(ThingActionInfo *info) {
    Thing *thing = info->thing();
    LwzModbusTcpConnection *connection = m_lwzConnections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcStiebelEltron()) << "Could not execute action. The modbus connection is currently not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    // Got this from StiebelEltron plugin, not sure if necessary
    if (thing->thingClassId() != stiebelEltronThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }

    if (info->action().actionTypeId() == stiebelEltronSgReadyActiveActionTypeId) {
        bool sgReadyActiveBool = info->action().paramValue(stiebelEltronSgReadyActiveActionSgReadyActiveParamTypeId).toBool();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        qCDebug(dcStiebelEltron()) << "Value: " << sgReadyActiveBool;

        QModbusReply *reply = connection->setSgReadyActive(sgReadyActiveBool);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyActiveBool] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron()) << "Set SG ready activation finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyActiveStateTypeId, sgReadyActiveBool);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    } else if (info->action().actionTypeId() == stiebelEltronSgReadyModeActionTypeId) {
        QString sgReadyModeString = info->action().paramValue(stiebelEltronSgReadyModeActionSgReadyModeParamTypeId).toString();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        LwzModbusTcpConnection::SmartGridState sgReadyState;
        if (sgReadyModeString == "Off") {
            sgReadyState = LwzModbusTcpConnection::SmartGridStateModeOne;
        } else if (sgReadyModeString == "Low") {
            sgReadyState = LwzModbusTcpConnection::SmartGridStateModeTwo;
        } else if (sgReadyModeString == "Standard") {
            sgReadyState = LwzModbusTcpConnection::SmartGridStateModeThree;
        } else if (sgReadyModeString == "High") {
            sgReadyState = LwzModbusTcpConnection::SmartGridStateModeFour;
        } else {
            qCWarning(dcStiebelEltron()) << "Failed to set SG Ready mode. An unknown SG Ready mode was passed: " << sgReadyModeString;
            info->finish(Thing::ThingErrorHardwareFailure);  // TODO better matching error type?
            return;
        }

        QModbusReply *reply = connection->setSgReadyState(sgReadyState);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyModeString] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron()) << "Set SG ready mode finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyModeStateTypeId, sgReadyModeString);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron()) << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginStiebelEltron::setupConnection(ThingSetupInfo *info) {
    Thing *thing = info->thing();
    uint controllerType = thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt();
    switch (controllerType) {
    case 103:
    case 104:
        qCDebug(dcStiebelEltron()) << "Controller type saved in config is" << controllerType << ", setting up connection for LWZ register map.";
        setupLwzConnection(info);
        break;
    case 390:
    case 391:
    case 449:
        qCDebug(dcStiebelEltron()) << "Controller type saved in config  is" << controllerType << ", setting up connection for WPM register map.";
        setupWpmConnection(info);
        break;
    default:
        qCWarning(dcStiebelEltron()) << "Failed to set up Stiebel Eltron heat pump because of invalid config parameter: controller type" << controllerType << ". Please reconfigure the device to fix this.";
        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The controller type is not vaild. Please reconfigure the device to fix this."));
        break;
    }
}

void IntegrationPluginStiebelEltron::setupWpmConnection(ThingSetupInfo *info) {

    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    WpmModbusTcpConnection *connection = new WpmModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcStiebelEltron()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "is now reachable at" << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_wpmConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(stiebelEltronConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcStiebelEltron()) << "The device is offline. Stopping communication attempts.";
                    m_wpmConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &WpmModbusTcpConnection::reachableChanged, thing, [this, thing, connection, monitor](bool reachable){
        qCDebug(dcStiebelEltron()) << "Reachable changed to" << reachable << "for" << thing << ". Monitor is" << monitor->reachable();

        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(stiebelEltronConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Stiebel Eltron does not have any relevant states, like the current electric power.

            // Check the monitor. If the monitor is reachable, get the current IP (maybe it changed) and reconnect.
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_wpmConnections.contains(thing)) {
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
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
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "has changed it's network address and is now reachable at" << networkDeviceInfo;
        } else {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "has changed it's network address and is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_wpmConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(stiebelEltronConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (connection->reachable == false).
                if (monitor->reachable()) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcStiebelEltron()) << "Connection is not working. Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcStiebelEltron()) << "The device is offline. Stopping communication attempts.";
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
                qCDebug(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - ok.";
                thing->setStateValue(stiebelEltronConnectedStateTypeId, true);
                break;
            default:
                qCWarning(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                             << thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
                return;
            }
        } else {
            qCDebug(dcStiebelEltron()) << "Initialization failed";
            thing->setStateValue(stiebelEltronConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Stiebel Eltron does not have any relevant states, like the current electric power.

            // Try once to reconnect the device
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_wpmConnections.contains(thing)) {
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                }
            }
        }
    });

    connect(connection, &WpmModbusTcpConnection::initializationFinished, info, [=](bool success){
        if (!success) {
            qCWarning(dcStiebelEltron()) << "Connection init finished with errors" << thing->name() << connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error communicating with the heat pump."));
            return;
        }

        qCDebug(dcStiebelEltron()) << "Connection init finished successfully" << connection;

        uint controllerType = connection->controllerType();
        switch (controllerType) {
        case 390:
        case 391:
        case 449:
            qCDebug(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - ok.";
            break;
        default:
            qCWarning(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                         << thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("Wrong configuration. Please reconfigure the device to fix this."));
            return;
        }

        m_wpmConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        thing->setStateValue(stiebelEltronConnectedStateTypeId, true);
        connection->update();
    });


    connect(connection, &WpmModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
        qCDebug(dcStiebelEltron()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
        thing->setStateValue(stiebelEltronOutdoorTemperatureStateTypeId, outdoorTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::flowTemperatureChanged, thing, [thing](float flowTemperature){
        qCDebug(dcStiebelEltron()) << thing << "flow temperature changed" << flowTemperature << "°C";
        thing->setStateValue(stiebelEltronFlowTemperatureStateTypeId, flowTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
        qCDebug(dcStiebelEltron()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
        thing->setStateValue(stiebelEltronHotWaterTemperatureStateTypeId, hotWaterTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::storageTankTemperatureChanged, thing, [thing](float storageTankTemperature){
        qCDebug(dcStiebelEltron()) << thing << "Storage tank temperature changed" << storageTankTemperature << "°C";
        thing->setStateValue(stiebelEltronStorageTankTemperatureStateTypeId, storageTankTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::returnTemperatureChanged, thing, [thing](float returnTemperature){
        qCDebug(dcStiebelEltron()) << thing << "return temperature changed" << returnTemperature << "°C";
        thing->setStateValue(stiebelEltronReturnTemperatureStateTypeId, returnTemperature);
    });

    connect(connection, &WpmModbusTcpConnection::heatingEnergyChanged, thing, [thing](quint32 heatingEnergy){
        // kWh and MWh of energy are stored in two registers an read as an uint32. The following arithmetic splits the uint32 into
        // two uint16 and sums up the MWh and kWh values.
        quint32 correctedEnergy = (heatingEnergy >> 16) + (heatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Heating energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronHeatingEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::hotWaterEnergyChanged, thing, [thing](quint32 hotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (hotWaterEnergy >> 16) + (hotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Hot Water energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronHotWaterEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::consumedEnergyHeatingChanged, thing, [thing](quint32 consumedEnergyHeatingEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHeatingEnergy >> 16) + (consumedEnergyHeatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Consumed energy Heating changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronConsumedEnergyHeatingStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::consumedEnergyHotWaterChanged, thing, [thing](quint32 consumedEnergyHotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHotWaterEnergy >> 16) + (consumedEnergyHotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Consumed energy hot water changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronConsumedEnergyHotWaterStateTypeId, correctedEnergy);
    });

    connect(connection, &WpmModbusTcpConnection::operatingModeChanged, thing, [thing](WpmModbusTcpConnection::OperatingModeWpm operatingMode){
        qCDebug(dcStiebelEltron()) << thing << "operating mode changed " << operatingMode;
        switch (operatingMode) {
        case WpmModbusTcpConnection::OperatingModeWpmEmergency:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Emergency");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmStandby:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Standby");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmProgram:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Program");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmComfort:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Comfort");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmEco:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Eco");
            break;
        case WpmModbusTcpConnection::OperatingModeWpmHotWater:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Hot water");
            break;
        }
    });

    connect(connection, &WpmModbusTcpConnection::systemStatusChanged, thing, [thing](uint16_t systemStatus){
        qCDebug(dcStiebelEltron()) << thing << "System status changed " << systemStatus;
        thing->setStateValue(stiebelEltronPumpOneStateTypeId, systemStatus & (1 << 0));
        thing->setStateValue(stiebelEltronPumpTwoStateTypeId, systemStatus & (1 << 1));
        thing->setStateValue(stiebelEltronHeatingUpStateTypeId, systemStatus & (1 << 2));
        thing->setStateValue(stiebelEltronAuxHeatingStateTypeId, systemStatus & (1 << 3));
        thing->setStateValue(stiebelEltronHeatingStateTypeId, systemStatus & (1 << 4));
        thing->setStateValue(stiebelEltronHotWaterStateTypeId, systemStatus & (1 << 5));
        thing->setStateValue(stiebelEltronCompressorStateTypeId, systemStatus & (1 << 6));
        thing->setStateValue(stiebelEltronSummerModeStateTypeId, systemStatus & (1 << 7));
        thing->setStateValue(stiebelEltronCoolingModeStateTypeId, systemStatus & (1 << 8));
        thing->setStateValue(stiebelEltronDefrostingStateTypeId, systemStatus & (1 << 9));
        thing->setStateValue(stiebelEltronSilentModeStateTypeId, systemStatus & (1 << 10));
        thing->setStateValue(stiebelEltronSilentMode2StateTypeId, systemStatus & (1 << 11));
    });

    connect(connection, &WpmModbusTcpConnection::sgReadyStateChanged, thing, [thing](WpmModbusTcpConnection::SmartGridState smartGridState){
        qCDebug(dcStiebelEltron()) << thing << "SG Ready mode changed" << smartGridState;
        switch (smartGridState) {
        case WpmModbusTcpConnection::SmartGridStateModeOne:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Off");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeTwo:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Low");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeThree:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Standard");
            break;
        case WpmModbusTcpConnection::SmartGridStateModeFour:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "High");
            break;
        }
    });

    connect(connection, &WpmModbusTcpConnection::sgReadyActiveChanged, thing, [thing](bool smartGridActive){
        qCDebug(dcStiebelEltron()) << thing << "SG Ready activation changed" << smartGridActive;
        thing->setStateValue(stiebelEltronSgReadyActiveStateTypeId, smartGridActive);
    });

    connection->connectDevice();
}

void IntegrationPluginStiebelEltron::setupLwzConnection(ThingSetupInfo *info) {

    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    LwzModbusTcpConnection *connection = new LwzModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcStiebelEltron()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "is now reachable at" << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_lwzConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(stiebelEltronConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_lwzConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_lwzConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcStiebelEltron()) << "The device is offline. Stopping communication attempts.";
                    m_lwzConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &LwzModbusTcpConnection::reachableChanged, thing, [this, thing, connection, monitor](bool reachable){
        qCDebug(dcStiebelEltron()) << "Reachable changed to" << reachable << "for" << thing << ". Monitor is" << monitor->reachable();

        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(stiebelEltronConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Stiebel Eltron does not have any relevant states, like the current electric power.

            // Check the monitor. If the monitor is reachable, get the current IP (maybe it changed) and reconnect.
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_lwzConnections.contains(thing)) {
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_lwzConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_lwzConnections.value(thing)->reconnectDevice();
                }
            } else {
                // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                m_lwzConnections.value(thing)->disconnectDevice();
            }
        }
    });

    // This is needed because the monitor is sometimes too slow to trigger reachableChanged when the address changed.
    connect(monitor, &NetworkDeviceMonitor::networkDeviceInfoChanged, thing, [=](NetworkDeviceInfo networkDeviceInfo){

        if (monitor->reachable()) {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "has changed it's network address and is now reachable at" << networkDeviceInfo;
        } else {
            qCDebug(dcStiebelEltron()) << "Monitor detected a change -" << thing << "has changed it's network address and is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_lwzConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(stiebelEltronConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (connection->reachable == false).
                if (monitor->reachable()) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcStiebelEltron()) << "Connection is not working. Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_lwzConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_lwzConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcStiebelEltron()) << "The device is offline. Stopping communication attempts.";
                    m_lwzConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &LwzModbusTcpConnection::initializationFinished, thing, [this, thing, connection, monitor](bool success){
        if (!thing->setupComplete())
            return;

        if (success) {
            uint controllerType = connection->controllerType();
            switch (controllerType) {
            case 103:
            case 104:
                qCDebug(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - ok.";
                thing->setStateValue(stiebelEltronConnectedStateTypeId, true);
                break;
            default:
                qCWarning(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                             << thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
                return;
            }
        } else {
            qCDebug(dcStiebelEltron()) << "Initialization failed";
            thing->setStateValue(stiebelEltronConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Stiebel Eltron does not have any relevant states, like the current electric power.

            // Try once to reconnect the device
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_lwzConnections.contains(thing)) {
                    qCDebug(dcStiebelEltron()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_lwzConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_lwzConnections.value(thing)->reconnectDevice();
                }
            }
        }
    });

    connect(connection, &LwzModbusTcpConnection::initializationFinished, info, [=](bool success){
        if (!success) {
            qCWarning(dcStiebelEltron()) << "Connection init finished with errors" << thing->name() << connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error communicating with the heat pump."));
            return;
        }

        qCDebug(dcStiebelEltron()) << "Connection init finished successfully" << connection;

        uint controllerType = connection->controllerType();
        switch (controllerType) {
        case 103:
        case 104:
            qCDebug(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - ok.";
            break;
        default:
            qCWarning(dcStiebelEltron()) << "Checking controller type:" << controllerType << " - Error, configured controller type"
                                         << thing->paramValue(stiebelEltronThingControllerTypeParamTypeId).toUInt() << "does not match heat pump! Please reconfigure the device to fix this.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            connection->deleteLater();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("Wrong configuration. Please reconfigure the device to fix this."));
            return;
        }

        m_lwzConnections.insert(thing, connection);
        info->finish(Thing::ThingErrorNoError);
        thing->setStateValue(stiebelEltronConnectedStateTypeId, true);
        connection->update();
    });


    connect(connection, &LwzModbusTcpConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
        qCDebug(dcStiebelEltron()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
        thing->setStateValue(stiebelEltronOutdoorTemperatureStateTypeId, outdoorTemperature);
    });

    connect(connection, &LwzModbusTcpConnection::flowTemperatureChanged, thing, [thing](float flowTemperature){
        qCDebug(dcStiebelEltron()) << thing << "flow temperature changed" << flowTemperature << "°C";
        thing->setStateValue(stiebelEltronFlowTemperatureStateTypeId, flowTemperature);
    });

    connect(connection, &LwzModbusTcpConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
        qCDebug(dcStiebelEltron()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
        thing->setStateValue(stiebelEltronHotWaterTemperatureStateTypeId, hotWaterTemperature);
    });

    // Lwz register mapping does not have storage tank temperature.

    connect(connection, &LwzModbusTcpConnection::returnTemperatureChanged, thing, [thing](float returnTemperature){
        qCDebug(dcStiebelEltron()) << thing << "return temperature changed" << returnTemperature << "°C";
        thing->setStateValue(stiebelEltronReturnTemperatureStateTypeId, returnTemperature);
    });

    connect(connection, &LwzModbusTcpConnection::heatingEnergyChanged, thing, [thing](quint32 heatingEnergy){
        // kWh and MWh of energy are stored in two registers an read as an uint32. The following arithmetic splits the uint32 into
        // two uint16 and sums up the MWh and kWh values.
        quint32 correctedEnergy = (heatingEnergy >> 16) + (heatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Heating energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronHeatingEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &LwzModbusTcpConnection::hotWaterEnergyChanged, thing, [thing](quint32 hotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (hotWaterEnergy >> 16) + (hotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Hot Water energy changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronHotWaterEnergyStateTypeId, correctedEnergy);
    });

    connect(connection, &LwzModbusTcpConnection::consumedEnergyHeatingChanged, thing, [thing](quint32 consumedEnergyHeatingEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHeatingEnergy >> 16) + (consumedEnergyHeatingEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Consumed energy Heating changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronConsumedEnergyHeatingStateTypeId, correctedEnergy);
    });

    connect(connection, &LwzModbusTcpConnection::consumedEnergyHotWaterChanged, thing, [thing](quint32 consumedEnergyHotWaterEnergy){
        // see comment in heatingEnergyChanged
        quint32 correctedEnergy = (consumedEnergyHotWaterEnergy >> 16) + (consumedEnergyHotWaterEnergy & 0xFFFF) * 1000;
        qCDebug(dcStiebelEltron()) << thing << "Consumed energy hot water changed" << correctedEnergy << "kWh";
        thing->setStateValue(stiebelEltronConsumedEnergyHotWaterStateTypeId, correctedEnergy);
    });

    connect(connection, &LwzModbusTcpConnection::operatingModeChanged, thing, [thing](LwzModbusTcpConnection::OperatingModeLwz operatingMode){
        qCDebug(dcStiebelEltron()) << thing << "operating mode changed " << operatingMode;
        switch (operatingMode) {
        case LwzModbusTcpConnection::OperatingModeLwzEmergency:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Emergency");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzStandby:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Standby");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzProgram:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Program");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzComfort:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Comfort");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzEco:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Eco");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzHotWater:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Hot water");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzAutomatic:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Automatic");
            break;
        case LwzModbusTcpConnection::OperatingModeLwzManual:
            thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Manual");
            break;
        }
    });

    connect(connection, &LwzModbusTcpConnection::systemStatusChanged, thing, [thing](uint16_t systemStatus){
        qCDebug(dcStiebelEltron()) << thing << "System status changed " << systemStatus;
        thing->setStateValue(stiebelEltronPumpOneStateTypeId, systemStatus & (1 << 0));
        thing->setStateValue(stiebelEltronPumpTwoStateTypeId, systemStatus & (1 << 1));
        thing->setStateValue(stiebelEltronHeatingUpStateTypeId, systemStatus & (1 << 2));
        thing->setStateValue(stiebelEltronAuxHeatingStateTypeId, systemStatus & (1 << 3));
        thing->setStateValue(stiebelEltronHeatingStateTypeId, systemStatus & (1 << 4));
        thing->setStateValue(stiebelEltronHotWaterStateTypeId, systemStatus & (1 << 5));
        thing->setStateValue(stiebelEltronCompressorStateTypeId, systemStatus & (1 << 6));
        thing->setStateValue(stiebelEltronSummerModeStateTypeId, systemStatus & (1 << 7));
        thing->setStateValue(stiebelEltronCoolingModeStateTypeId, systemStatus & (1 << 8));
        thing->setStateValue(stiebelEltronDefrostingStateTypeId, systemStatus & (1 << 9));
        thing->setStateValue(stiebelEltronSilentModeStateTypeId, systemStatus & (1 << 10));
        thing->setStateValue(stiebelEltronSilentMode2StateTypeId, systemStatus & (1 << 11));
    });

    connect(connection, &LwzModbusTcpConnection::sgReadyStateChanged, thing, [thing](LwzModbusTcpConnection::SmartGridState smartGridState){
        qCDebug(dcStiebelEltron()) << thing << "SG Ready mode changed" << smartGridState;
        switch (smartGridState) {
        case LwzModbusTcpConnection::SmartGridStateModeOne:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Off");
            break;
        case LwzModbusTcpConnection::SmartGridStateModeTwo:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Low");
            break;
        case LwzModbusTcpConnection::SmartGridStateModeThree:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Standard");
            break;
        case LwzModbusTcpConnection::SmartGridStateModeFour:
            thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "High");
            break;
        }
    });

    connect(connection, &LwzModbusTcpConnection::sgReadyActiveChanged, thing, [thing](bool smartGridActive){
        qCDebug(dcStiebelEltron()) << thing << "SG Ready activation changed" << smartGridActive;
        thing->setStateValue(stiebelEltronSgReadyActiveStateTypeId, smartGridActive);
    });

    connection->connectDevice();
}
