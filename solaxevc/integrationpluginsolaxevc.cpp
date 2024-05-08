/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2022, nymea GmbH
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

#include "integrationpluginsolaxevc.h"
#include "plugininfo.h"
#include "discoverytcp.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <QEventLoop>

IntegrationPluginSolaxEvc::IntegrationPluginSolaxEvc()
{

}

void IntegrationPluginSolaxEvc::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxEvcThingClassId)
    {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcSolaxEvc()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        SolaxEvcTCPDiscovery *discovery = new SolaxEvcTCPDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &SolaxEvcTCPDiscovery::discoveryFinished, info, [=](){
            foreach (const SolaxEvcTCPDiscovery::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(solaxEvcThingClassId, "Solax Wallbox", result.networkDeviceInfo.address().toString());
                qCInfo(dcSolaxEvc()) << "Discovered:" << descriptor.title() << descriptor.description();

                // TODO: CHECK IF RECONFIGURE WORKING
                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Things existingThings = myThings().filterByThingClassId(solaxEvcThingClassId).filterByParam(solaxEvcThingModbusIdParamTypeId, result.networkDeviceInfo.macAddress());
                if (!existingThings.isEmpty())
                    descriptor.setThingId(existingThings.first()->id());

                ParamList params;
                params << Param(solaxEvcThingIpAddressParamTypeId, result.networkDeviceInfo.address().toString());
                params << Param(solaxEvcThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                params << Param(solaxEvcThingPortParamTypeId, result.port);
                params << Param(solaxEvcThingModbusIdParamTypeId, result.modbusId);
                descriptor.setParams(params);
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        // Start the discovery process
        discovery->startDiscovery();
    }
}

void IntegrationPluginSolaxEvc::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolaxEvc()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == solaxEvcThingClassId)
    {

        // Handle reconfigure
        if (m_tcpConnections.contains(thing))
        {
            qCDebug(dcSolaxEvc()) << "Already have a Solax connection for this thing. Cleaning up old connection and initializing new one...";
            SolaxEvcModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        } else {
            qCDebug(dcSolaxEvc()) << "Setting up a new device: " << thing->params();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is possible.
        // Testing for null is necessary, because registering a monitor with a zero mac adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(solaxEvcThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcSolaxEvc()) << "Failed to set up Solax wallbox because the MAC address is not valid:" << thing->paramValue(solaxEvcThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is reachable without polling on our own.
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // TODO: besseren Fix from solax inverter
        if (monitor->networkDeviceInfo().address().toString() == "")
        {
            QEventLoop loop;
            connect(hardwareManager()->networkDeviceDiscovery(), &NetworkDeviceDiscovery::cacheUpdated, &loop, &QEventLoop::quit);
            loop.exec();
        }
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            // Is this needed? How can setup be aborted at this point?

            // Clean up in case the setup gets aborted.
            if (m_monitors.contains(thing)) {
                qCDebug(dcSolaxEvc()) << "Unregister monitor because the setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        uint port = thing->paramValue(solaxEvcThingPortParamTypeId).toUInt();
        quint16 modbusId = thing->paramValue(solaxEvcThingModbusIdParamTypeId).toUInt();
        SolaxEvcModbusTcpConnection *connection = new SolaxEvcModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        m_tcpConnections.insert(thing, connection);

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
            qCDebug(dcSolaxEvc()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxEvcConnectedStateTypeId).toBool())
            {
                connection->setHostAddress(monitor->networkDeviceInfo().address());
                connection->reconnectDevice();
            } else {
                // Note: We disable autoreconnect explicitly and we will
                // connect the device once the monitor says it is reachable again
                connection->disconnectDevice();
            }
        });

        connect(connection, &SolaxEvcModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable) {
            qCDebug(dcSolaxEvc()) << "Reachable state changed to" << reachable;
            if (reachable)
            {
                // Connected true will be set after successfull init.
                connection->initialize();
            } else {
                thing->setStateValue(solaxEvcConnectedStateTypeId, false);
                thing->setStateValue(solaxEvcCurrentPowerStateTypeId, 0);
            }
        });

        connect(connection, &SolaxEvcModbusTcpConnection::initializationFinished, thing, [=](bool success) {
            thing->setStateValue(solaxEvcConnectedStateTypeId, success);

            if (success)
            {
                qCDebug(dcSolaxEvc()) << "Solax wallbox initialized.";
                thing->setStateValue(solaxEvcFirmwareVersionStateTypeId, connection->firmwareVersion());
                thing->setStateValue(solaxEvcSerialNumberStateTypeId, connection->serialNumber());
            } else {
                qCDebug(dcSolaxEvc()) << "Solax wallbox initialization failed.";
                // Try to reconnect to device
                connection->reconnectDevice();
            }
        });

        // connect current charging power
        connect(connection, &SolaxEvcModbusTcpConnection::totalPowerChanged, thing, [thing](quint16 totalPower) {
            qCDebug(dcSolaxEvc()) << "Total charging power changed" << totalPower << "W";
            thing->setStateValue(solaxEvcCurrentPowerStateTypeId, totalPower);
        });

        // connect energy consumption of current session
        connect(connection, &SolaxEvcModbusTcpConnection::sessionEnergyChanged, thing, [thing](double sessionEnergy) {
            qCDebug(dcSolaxEvc()) << "Session energy changed" << sessionEnergy << "kWh";
            thing->setStateValue(solaxEvcSessionEnergyStateTypeId, sessionEnergy);
        });

        // connect total energy consumption
        connect(connection, &SolaxEvcModbusTcpConnection::totalEnergyChanged, thing, [thing](double totalEnergy) {
            qCDebug(dcSolaxEvc()) << "Total energy changed" << totalEnergy << "kWh";
            thing->setStateValue(solaxEvcTotalEnergyConsumedStateTypeId, totalEnergy);
        });

        // connect time of current charging session
        connect(connection, &SolaxEvcModbusTcpConnection::chargingTimeChanged, thing, [thing](quint32 time) {
            qCDebug(dcSolaxEvc()) << "Charging Time changed" << time << "s";
            // Every once in a while, nonsense values are received, charging session unlikely to be above 55h
            if (time < 200000)
                thing->setStateValue(solaxEvcChargingTimeStateTypeId, time);
        });

        // connect max charging current
        connect(connection, &SolaxEvcModbusTcpConnection::MaxCurrentChanged, thing, [thing](float maxCurrent) {
            qCDebug(dcSolaxEvc()) << "Max current changed" << maxCurrent << "A";
            // Every once in a while, nonsense values are received, only change when value between 6 and 16 received
            if (maxCurrent >= 6 && maxCurrent <= 16)
                thing->setStateValue(solaxEvcMaxChargingCurrentStateTypeId, maxCurrent);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::deviceModeChanged, thing, [this, thing, connection](quint16 mode) {
            qCDebug(dcSolaxEvc()) << "Device mode changed" << mode;
            if (thing->stateValue(solaxEvcPowerStateTypeId).toBool() == true)
            {
                if (mode == 1)
                {
                    toggleCharging(connection, true); 
                } else if (mode == 0) {
                    toggleCharging(connection, false);
                }
            } else {
                qCDebug(dcSolaxEvc()) << "EV Plugged In; Charging disabled! Make sure the wallbox does not charge.";
                toggleCharging(connection, false);
            }
        });

        connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseAChanged, thing, [thing](double currentPhase) {
            qCDebug(dcSolaxEvc()) << "Current Phase" << currentPhase << "A";
            thing->setStateValue(solaxEvcCurrentPhaseAStateTypeId, currentPhase);
        });
        connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseBChanged, thing, [thing](double currentPhase) {
            qCDebug(dcSolaxEvc()) << "Current Phase" << currentPhase << "A";
            thing->setStateValue(solaxEvcCurrentPhaseBStateTypeId, currentPhase);
        });
        connect(connection, &SolaxEvcModbusTcpConnection::currentPhaseCChanged, thing, [thing](double currentPhase) {
            qCDebug(dcSolaxEvc()) << "Current Phase" << currentPhase << "A";
            thing->setStateValue(solaxEvcCurrentPhaseCStateTypeId, currentPhase);
        });

        // connect current state of evcharger
        connect(connection, &SolaxEvcModbusTcpConnection::stateChanged, thing, [thing](quint16 state) {
            qCDebug(dcSolaxEvc()) << "State changed" << state;
            // thing->setStateValue(solaxEvcStateStateTypeId, state);

            switch (state)
            {
                case SolaxEvcModbusTcpConnection::StateAvailable:
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Available");
                    break;
                case SolaxEvcModbusTcpConnection::StatePreparing:
                    thing->setStateValue(solaxEvcStateStateTypeId, "Preparing");
                    break;
                case SolaxEvcModbusTcpConnection::StateCharging:
                    thing->setStateValue(solaxEvcChargingStateTypeId, true);
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Charging");
                    break;
                case SolaxEvcModbusTcpConnection::StateFinishing:
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Plugged In");
                    break;
                case SolaxEvcModbusTcpConnection::StateFaulted:
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Faulted");
                    break;
                case SolaxEvcModbusTcpConnection::StateUnavailable:
                    thing->setStateValue(solaxEvcStateStateTypeId, "Unavailable");
                    break;
                case SolaxEvcModbusTcpConnection::StateReserved:
                    thing->setStateValue(solaxEvcStateStateTypeId, "Reserved");
                    break;
                case SolaxEvcModbusTcpConnection::StateSuspendedEV:
                    thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEV");
                    break;
                case SolaxEvcModbusTcpConnection::StateSuspendedEVSE:
                    thing->setStateValue(solaxEvcStateStateTypeId, "SuspendedEVSE");
                    break;
                case SolaxEvcModbusTcpConnection::StateUpdate:
                    thing->setStateValue(solaxEvcStateStateTypeId, "Update");
                    break;
                case SolaxEvcModbusTcpConnection::StateCardActivation:
                    thing->setStateValue(solaxEvcStateStateTypeId, "Card Activation");
                    break;
                default:
                    qCWarning(dcSolaxEvc()) << "State changed to unknown value";
                    thing->setStateValue(solaxEvcStateStateTypeId, "Unknown");
                    break;
            }
        });

        // connect fault code
        connect(connection, &SolaxEvcModbusTcpConnection::faultCodeChanged, thing, [thing](quint32 code) {
            qCDebug(dcSolaxEvc()) << "Fault code changed" << code;
            QMap<int, QString> faultCodeMap = {{0,"PowerSelect_Fault (0)"},{1,"Emergency Stop (1)"},{2,"Overvoltage L1 (2)"},{3,"Undervoltage L1 (3)"}, \
                                               {4,"Overvoltage L2 (4)"},{5,"Undervoltage L2 (5)"},{6,"Overvoltage L3 (6)"},{7,"Undervoltage L2 (7)"},{8,"Electronic Lock (8)"}, \
                                               {9,"Over Load (9)"},{10,"Over Current (10)"},{11,"Over Temperature (11)"},{12,"PE Ground (12)"},{13,"PE Leak Current (13)"}, \
                                               {14,"Over Leak Current (14)"},{15,"Meter Communication (15)"},{16,"485 Communication (16)"},{17,"CP Voltage (17)"}};
            if (thing->stateValue(solaxEvcStateStateTypeId).toString() == "Faulted")
            {
                thing->setStateValue(solaxEvcFaultCodeStateTypeId, faultCodeMap[code]);
            } else {
                thing->setStateValue(solaxEvcFaultCodeStateTypeId, "No Error");
            }
        });

        // connect current charging phases
        connect(connection, &SolaxEvcModbusTcpConnection::chargePhaseChanged, thing, [thing](quint16 phaseCount) {
            qCDebug(dcSolaxEvc()) << "Phase count changed" << phaseCount;
            // TODO: Test if working properly
            thing->setStateValue(solaxEvcPhaseCountStateTypeId, phaseCount == 0 ? 3 : 1);
        });

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSolaxEvc::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcSolaxEvc()) << "Post setup thing..";

    if (!m_pluginTimer)
    {
        qCDebug(dcSolaxEvc()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcSolaxEvc()) << "Updating Solax EVC..";
            foreach (SolaxEvcModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginSolaxEvc::executeAction(ThingActionInfo *info)
{
    // TODO: needs to be properly tested, when modbus communication is working properly
    Thing *thing = info->thing();
    if (thing->thingClassId() == solaxEvcThingClassId)
    {
        SolaxEvcModbusTcpConnection *connection = m_tcpConnections.value(thing);

        // set the maximum charging current
        if (info->action().actionTypeId() == solaxEvcMaxChargingCurrentActionTypeId)
        {
            // set maximal charging current
            quint32 maxCurrent = info->action().paramValue(solaxEvcMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            QModbusReply *reply = connection->setMaxCurrent(maxCurrent);
            connect(reply, &QModbusReply::finished, thing, [info, thing, reply, maxCurrent]() {
                if (reply->error() == QModbusDevice::NoError)
                {
                    thing->setStateValue(solaxEvcMaxChargingCurrentStateTypeId, maxCurrent);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcSolaxEvc()) << "Error setting maximal charging current:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

        // toggle charging enabled
        if (info->action().actionTypeId() == solaxEvcPowerActionTypeId)
        {
            // start / stop the charging session
            bool power = info->action().paramValue(solaxEvcPowerActionPowerParamTypeId).toBool();
            QModbusReply *reply = connection->setControlCommand(power ? SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStartCharging : SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStopCharging);
            connect(reply, &QModbusReply::finished, thing, [this, info, thing, reply, power]() {
                if (reply->error() == QModbusDevice::NoError)
                {
                    thing->setStateValue(solaxEvcPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcSolaxEvc()) << "Error setting charging state: " << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
    }
}

void IntegrationPluginSolaxEvc::toggleCharging(SolaxEvcModbusTcpConnection *connection, bool power)
{
    QModbusReply *reply = connection->setControlCommand(power ? SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStartCharging : SolaxEvcModbusTcpConnection::ControlCommand::ControlCommandStopCharging);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, reply, connection, power]() {
        if (reply->error() != QModbusDevice::NoError)
        {
            qCWarning(dcSolaxEvc()) << "Error setting charging state: " << reply->error() << reply->errorString();
            toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginSolaxEvc::thingRemoved(Thing *thing)
{
    qCDebug(dcSolaxEvc()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing))
    {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing))
    {
        SolaxEvcModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer)
    {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
