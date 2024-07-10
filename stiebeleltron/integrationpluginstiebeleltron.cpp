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
                foreach (StiebelEltronModbusTcpConnection *connection, m_connections) {
                    if (connection->connected()) {
                        connection->update();
                    }
                }
            });

            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginStiebelEltron::thingRemoved(Thing *thing) {
    if (thing->thingClassId() == stiebelEltronThingClassId && m_connections.contains(thing)) {
        m_connections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginStiebelEltron::executeAction(ThingActionInfo *info) {
    Thing *thing = info->thing();
    StiebelEltronModbusTcpConnection *connection = m_connections.value(thing);

    if (!connection->connected()) {
        qCWarning(dcStiebelEltron()) << "Could not execute action. The modbus connection is currently "
                                        "not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    // Got this from StiebelEltron plugin, not sure if necessary
    if (thing->thingClassId() != stiebelEltronThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }

    if (info->action().actionTypeId() == stiebelEltronSgReadyActiveActionTypeId) {
        bool sgReadyActiveBool =
            info->action().paramValue(stiebelEltronSgReadyActiveActionSgReadyActiveParamTypeId).toBool();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString()
                                   << info->action().params();
        qCDebug(dcStiebelEltron()) << "Value: " << sgReadyActiveBool;

        QModbusReply *reply = connection->setSgReadyActive(sgReadyActiveBool);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the "
                                            "reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyActiveBool] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron())
                    << "Set SG ready activation finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully"
                                       << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyActiveStateTypeId, sgReadyActiveBool);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron())
                << "Modbus reply error occurred while execute action" << error << reply->errorString();
            emit reply->finished();  // To make sure it will be deleted
        });
    } else if (info->action().actionTypeId() == stiebelEltronSgReadyModeActionTypeId) {
        QString sgReadyModeString =
            info->action().paramValue(stiebelEltronSgReadyModeActionSgReadyModeParamTypeId).toString();
        qCDebug(dcStiebelEltron()) << "Execute action" << info->action().actionTypeId().toString()
                                   << info->action().params();
        StiebelEltronModbusTcpConnection::SmartGridState sgReadyState;
        if (sgReadyModeString == "Off") {
            sgReadyState = StiebelEltronModbusTcpConnection::SmartGridStateModeOne;
        } else if (sgReadyModeString == "Low") {
            sgReadyState = StiebelEltronModbusTcpConnection::SmartGridStateModeTwo;
        } else if (sgReadyModeString == "Standard") {
            sgReadyState = StiebelEltronModbusTcpConnection::SmartGridStateModeThree;
        } else if (sgReadyModeString == "High") {
            sgReadyState = StiebelEltronModbusTcpConnection::SmartGridStateModeFour;
        } else {
            qCWarning(dcStiebelEltron())
                << "Failed to set SG Ready mode. An unknown SG Ready mode was passed: " << sgReadyModeString;
            info->finish(Thing::ThingErrorHardwareFailure);  // TODO better matching error type?
            return;
        }

        QModbusReply *reply = connection->setSgReadyState(sgReadyState);
        if (!reply) {
            qCWarning(dcStiebelEltron()) << "Execute action failed because the "
                                            "reply could not be created.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply, sgReadyModeString] {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcStiebelEltron())
                    << "Set SG ready mode finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcStiebelEltron()) << "Execute action finished successfully"
                                       << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(stiebelEltronSgReadyModeStateTypeId, sgReadyModeString);
            info->finish(Thing::ThingErrorNoError);
        });

        connect(reply, &QModbusReply::errorOccurred, this, [reply](QModbusDevice::Error error) {
            qCWarning(dcStiebelEltron())
                << "Modbus reply error occurred while execute action" << error << reply->errorString();
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
        qCDebug(dcStiebelEltron()) << "Controller type is" << controllerType << ", setting up connection for LWZ register map.";
        setupLwzConnection(info);
        break;
    case 390:
    case 391:
    case 449:
        qCDebug(dcStiebelEltron()) << "Controller type is" << controllerType << ", setting up connection for WPM register map.";
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

    // Use a monitor to detect IP address changes. However, the monitor is not reliable and regularly reports 'not reachable' when the modbus connection
    // is still working. Because of that, check if the modbus connection is working before setting it to disconnect.
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [this, thing, monitor](bool reachable){
        // This may trigger before setup finished, but should only execute after setup. Check m_tcpConnections for thing, because that key won't
        // be in there before setup finished.
        if (m_wpmConnections.contains(thing)) {
            qCDebug(dcStiebelEltron()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->stateValue(stiebelEltronConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work. This code should not execute when modbus is still working.
                if (reachable) {
                    m_wpmConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_wpmConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    m_wpmConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });



    connect(connection, &StiebelEltronModbusTcpConnection::connectionStateChanged, thing,
            [thing, connection](bool status) {
                qCDebug(dcStiebelEltron()) << "Connected changed to" << status << "for" << thing;
                if (status) {
                    connection->update();
                }

                thing->setStateValue(stiebelEltronConnectedStateTypeId, status);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::outdoorTemperatureChanged, thing,
            [thing](float outdoorTemperature) {
                qCDebug(dcStiebelEltron())
                    << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
                thing->setStateValue(stiebelEltronOutdoorTemperatureStateTypeId, outdoorTemperature);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::flowTemperatureChanged, thing,
            [thing](float flowTemperature) {
                qCDebug(dcStiebelEltron())
                    << thing << "flow temperature changed" << flowTemperature << "°C";
                thing->setStateValue(stiebelEltronFlowTemperatureStateTypeId, flowTemperature);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::hotWaterTemperatureChanged, thing,
            [thing](float hotWaterTemperature) {
                qCDebug(dcStiebelEltron())
                    << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
                thing->setStateValue(stiebelEltronHotWaterTemperatureStateTypeId, hotWaterTemperature);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::storageTankTemperatureChanged, thing,
            [thing](float storageTankTemperature) {
                qCDebug(dcStiebelEltron())
                    << thing << "Storage tank temperature changed" << storageTankTemperature << "°C";
                thing->setStateValue(stiebelEltronStorageTankTemperatureStateTypeId,
                                     storageTankTemperature);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::returnTemperatureChanged, thing,
            [thing](float returnTemperature) {
                qCDebug(dcStiebelEltron())
                    << thing << "return temperature changed" << returnTemperature << "°C";
                thing->setStateValue(stiebelEltronReturnTemperatureStateTypeId, returnTemperature);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::heatingEnergyChanged, thing,
            [thing](quint32 heatingEnergy) {
                // kWh and MWh of energy are stored in two registers an read as
                // an uint32. The following arithmetic splits the uint32 into
                // two uint16 and sums up the MWh and kWh values.
                quint32 correctedEnergy = (heatingEnergy >> 16) + (heatingEnergy & 0xFFFF) * 1000;
                qCDebug(dcStiebelEltron())
                    << thing << "Heating energy changed" << correctedEnergy << "kWh";
                thing->setStateValue(stiebelEltronHeatingEnergyStateTypeId, correctedEnergy);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::hotWaterEnergyChanged, thing,
            [thing](quint32 hotWaterEnergy) {
                // see comment in heatingEnergyChanged
                quint32 correctedEnergy = (hotWaterEnergy >> 16) + (hotWaterEnergy & 0xFFFF) * 1000;
                qCDebug(dcStiebelEltron())
                    << thing << "Hot Water energy changed" << correctedEnergy << "kWh";
                thing->setStateValue(stiebelEltronHotWaterEnergyStateTypeId, correctedEnergy);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::consumedEnergyHeatingChanged, thing,
            [thing](quint32 consumedEnergyHeatingEnergy) {
                // see comment in heatingEnergyChanged
                quint32 correctedEnergy =
                    (consumedEnergyHeatingEnergy >> 16) + (consumedEnergyHeatingEnergy & 0xFFFF) * 1000;
                qCDebug(dcStiebelEltron())
                    << thing << "Consumed energy Heating changed" << correctedEnergy << "kWh";
                thing->setStateValue(stiebelEltronConsumedEnergyHeatingStateTypeId, correctedEnergy);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::consumedEnergyHotWaterChanged, thing,
            [thing](quint32 consumedEnergyHotWaterEnergy) {
                // see comment in heatingEnergyChanged
                quint32 correctedEnergy =
                    (consumedEnergyHotWaterEnergy >> 16) + (consumedEnergyHotWaterEnergy & 0xFFFF) * 1000;
                qCDebug(dcStiebelEltron())
                    << thing << "Consumed energy hot water changed" << correctedEnergy << "kWh";
                thing->setStateValue(stiebelEltronConsumedEnergyHotWaterStateTypeId, correctedEnergy);
            });

    connect(connection, &StiebelEltronModbusTcpConnection::operatingModeChanged, thing,
            [thing](StiebelEltronModbusTcpConnection::OperatingMode operatingMode) {
                qCDebug(dcStiebelEltron()) << thing << "operating mode changed " << operatingMode;
                switch (operatingMode) {
                    case StiebelEltronModbusTcpConnection::OperatingModeEmergency:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Emergency");
                        break;
                    case StiebelEltronModbusTcpConnection::OperatingModeStandby:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Standby");
                        break;
                    case StiebelEltronModbusTcpConnection::OperatingModeProgram:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Program");
                        break;
                    case StiebelEltronModbusTcpConnection::OperatingModeComfort:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Comfort");
                        break;
                    case StiebelEltronModbusTcpConnection::OperatingModeEco:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Eco");
                        break;
                    case StiebelEltronModbusTcpConnection::OperatingModeHotWater:
                        thing->setStateValue(stiebelEltronOperatingModeStateTypeId, "Hot water");
                        break;
                }
            });

    connect(connection, &StiebelEltronModbusTcpConnection::systemStatusChanged, thing,
            [thing](uint16_t systemStatus) {
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

    connect(connection, &StiebelEltronModbusTcpConnection::sgReadyStateChanged, thing,
            [thing](StiebelEltronModbusTcpConnection::SmartGridState smartGridState) {
                qCDebug(dcStiebelEltron()) << thing << "SG Ready mode changed" << smartGridState;
                switch (smartGridState) {
                    case StiebelEltronModbusTcpConnection::SmartGridStateModeOne:
                        thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Off");
                        break;
                    case StiebelEltronModbusTcpConnection::SmartGridStateModeTwo:
                        thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Low");
                        break;
                    case StiebelEltronModbusTcpConnection::SmartGridStateModeThree:
                        thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "Standard");
                        break;
                    case StiebelEltronModbusTcpConnection::SmartGridStateModeFour:
                        thing->setStateValue(stiebelEltronSgReadyModeStateTypeId, "High");
                        break;
                }
            });
    connect(connection, &StiebelEltronModbusTcpConnection::sgReadyActiveChanged, thing,
            [thing](bool smartGridActive) {
                qCDebug(dcStiebelEltron()) << thing << "SG Ready activation changed" << smartGridActive;
                thing->setStateValue(stiebelEltronSgReadyActiveStateTypeId, smartGridActive);
            });

    m_connections.insert(thing, connection);
    connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginStiebelEltron::setupLwzConnection(ThingSetupInfo *info) {

}
