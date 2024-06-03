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

#include "integrationpluginamperfied.h"
#include "plugininfo.h"
#include "energycontroldiscovery.h"
#include "connecthomediscovery.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>

IntegrationPluginAmperfied::IntegrationPluginAmperfied()
{

}

void IntegrationPluginAmperfied::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == energyControlThingClassId) {
        EnergyControlDiscovery *discovery = new EnergyControlDiscovery(hardwareManager()->modbusRtuResource(), info);

        connect(discovery, &EnergyControlDiscovery::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with appropriate settings found. Please set up a modbus RTU master with a baudrate of 19200, 8 data bits, 1 stop bit and even parity first."));
                return;
            }

            qCInfo(dcAmperfied()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const EnergyControlDiscovery::Result &result, discovery->discoveryResults()) {

                ThingDescriptor descriptor(energyControlThingClassId, "Amperfied Energy Control", QString("Modbus ID: %1").arg(result.modbusId) + " on " + result.serialPort);

                ParamList params{
                    {energyControlThingRtuMasterParamTypeId, result.modbusRtuMasterId},
                    {energyControlThingModbusIdParamTypeId, result.modbusId},
                    {energyControlThingSerialNumberParamTypeId, result.serialNumber}
                };
                descriptor.setParams(params);


                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Things existingThings = myThings().filterByThingClassId(energyControlThingClassId).filterByParam(energyControlThingSerialNumberParamTypeId, result.serialNumber);
                if (!existingThings.isEmpty()) {
                    descriptor.setThingId(existingThings.first()->id());
                }
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

        discovery->startDiscovery();
        return;
    }

    if (info->thingClassId() == connectHomeThingClassId) {
        ConnectHomeDiscovery *discovery = new ConnectHomeDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &ConnectHomeDiscovery::discoveryFinished, info, [this, info, discovery](){
            qCInfo(dcAmperfied()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const ConnectHomeDiscovery::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(connectHomeThingClassId, "Amperfied connect.home", QString("MAC: %1").arg(result.networkDeviceInfo.macAddress()));

                ParamList params{
                    {connectHomeThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress()}
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

void IntegrationPluginAmperfied::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAmperfied()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == energyControlThingClassId) {

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcAmperfied()) << "Reconfiguring existing thing" << thing->name();
            m_rtuConnections.take(thing)->deleteLater();
        }

        setupRtuConnection(info);
        return;
    }


    if (thing->thingClassId() == connectHomeThingClassId) {
        m_setupTcpConnectionRunning = false;

        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcAmperfied()) << "Reconfiguring existing thing" << thing->name();
            AmperfiedModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice(); // Make sure it does not interfere with new connection we are about to create.
            connection->deleteLater();
        }

        if (m_monitors.contains(thing))
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

        // Test for null, because registering a monitor with null will cause a segfault. Mac is loaded from config, you can't be sure config contains a valid mac.
        MacAddress macAddress = MacAddress(thing->paramValue(connectHomeThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcAmperfied()) << "Failed to set up Amperfied connect.home because the MAC address is not valid:" << thing->paramValue(connectHomeThingMacAddressParamTypeId).toString() << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device to fix this."));
            return;
        }

        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcAmperfied()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // During Nymea startup, it can happen that the monitor is not yet ready when this code triggers. Not ready means, the monitor has not yet scanned the network and
        // does not yet have a mapping of mac<->IP. Because of this, the monitor can't give an IP address when asked for, and setup would fail. To circumvent this problem,
        // wait for the monitor to signal "reachable". When the monitor emits this signal, it is ready to give an IP address.
        qCDebug(dcAmperfied()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(connectHomeThingMacAddressParamTypeId).toString();
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info](bool reachable){
                qCDebug(dcAmperfied()) << "Monitor reachable changed!" << reachable;
                if (reachable && !m_setupTcpConnectionRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_setupTcpConnectionRunning = true;
                    setupTcpConnection(info);
                }
            });
        }
    }
}

void IntegrationPluginAmperfied::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    if (!m_pluginTimer) {
        qCDebug(dcAmperfied()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(AmperfiedEnergyControlModbusRtuConnection *connection, m_rtuConnections) {
                qCDebug(dcAmperfied()) << "Updating connection" << connection->modbusRtuMaster() << connection->slaveId();
                connection->update();
            }
            foreach(AmperfiedModbusTcpConnection *connection, m_tcpConnections) {
                qCDebug(dcAmperfied()) << "Updating connection" << connection->hostAddress();
                connection->update();
            }
        });
        m_pluginTimer->start();
    }
}

void IntegrationPluginAmperfied::executeAction(ThingActionInfo *info)
{
    if (info->thing()->thingClassId() == energyControlThingClassId) {
        AmperfiedEnergyControlModbusRtuConnection *connection = m_rtuConnections.value(info->thing());

        if (info->action().actionTypeId() == energyControlPowerActionTypeId) {
            bool power = info->action().paramValue(energyControlPowerActionPowerParamTypeId).toBool();
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? info->thing()->stateValue(energyControlMaxChargingCurrentStateTypeId).toUInt() * 10 : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, power](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(energyControlPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAmperfied()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
            return;
        }

        if (info->action().actionTypeId() == energyControlMaxChargingCurrentActionTypeId) {
            bool power = info->thing()->stateValue(energyControlPowerStateTypeId).toBool();
            uint max = info->action().paramValue(energyControlMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt() * 10;
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, max](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(energyControlMaxChargingCurrentStateTypeId, max / 10);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAmperfied()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
    }

    if (info->thing()->thingClassId() == connectHomeThingClassId) {
        AmperfiedModbusTcpConnection *connection = m_tcpConnections.value(info->thing());

        if (info->action().actionTypeId() == connectHomePowerActionTypeId) {
            bool power = info->action().paramValue(connectHomePowerActionPowerParamTypeId).toBool();
            QModbusReply *reply = connection->setChargingCurrent(power ? info->thing()->stateValue(connectHomeMaxChargingCurrentStateTypeId).toUInt() * 10 : 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(connectHomePowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAmperfied()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
            return;
        }

        if (info->action().actionTypeId() == connectHomeMaxChargingCurrentActionTypeId) {
            bool power = info->thing()->stateValue(connectHomePowerStateTypeId).toBool();
            uint max = info->action().paramValue(connectHomeMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt() * 10;
            QModbusReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, max](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(connectHomeMaxChargingCurrentStateTypeId, max / 10);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAmperfied()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
    }
}

void IntegrationPluginAmperfied::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == energyControlThingClassId) {
        if (m_rtuConnections.contains(thing))
            m_rtuConnections.take(thing)->deleteLater();
    }

    if (thing->thingClassId() == connectHomeThingClassId) {
        if (m_monitors.contains(thing)) {
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
        if (m_tcpConnections.contains(thing)) {
            AmperfiedModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice(); // This is needed to avoid a segfault.
            connection->deleteLater();
        }
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginAmperfied::setupRtuConnection(ThingSetupInfo *info)
{
    // This setup method checks the version number of the wallbox before giving the signal that info finished successfully. The version number is read
    // from a modbus register. Modbus calls can fail, making the setup fail. As a result, care needs to be taken to clean up all objects that were created.

    Thing *thing = info->thing();
    ModbusRtuMaster *master = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(thing->paramValue(energyControlThingRtuMasterParamTypeId).toUuid());
    if (!master) {
        qCWarning(dcAmperfied()) << "The Modbus Master is not available any more.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The modbus RTU connection is not available."));
        return;
    }
    quint16 modbusId = thing->paramValue(energyControlThingModbusIdParamTypeId).toUInt();

    // The parent object given to the connection object should be thing. The point of a parent relation is to delete all children when the parent gets deleted.
    // The connection object should be deleted when the thing gets deleted, to avoid segfault problems.
    AmperfiedEnergyControlModbusRtuConnection *connection = new AmperfiedEnergyControlModbusRtuConnection(master, modbusId, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcAmperfied()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
        connection->deleteLater();
    });

    connect(connection, &AmperfiedEnergyControlModbusRtuConnection::reachableChanged, thing, [connection, thing](bool reachable){
        if (reachable) {
            connection->initialize();
        } else {
            qCDebug(dcAmperfied()) << "The wallbox is not reachable anymore. Connected changed to false.";
            thing->setStateValue(energyControlConnectedStateTypeId, false);
            thing->setStateValue(energyControlCurrentPowerStateTypeId, 0);
        }
    });
    connect(connection, &AmperfiedEnergyControlModbusRtuConnection::initializationFinished, thing, [connection, thing](bool success){
        if (success) {
            QString serialNumberRead{connection->serialNumber()};
            QString serialNumberConfig{thing->paramValue(energyControlThingSerialNumberParamTypeId).toString()};
            if (serialNumberRead != serialNumberConfig) {
                // The wallbox found is a different one than configured. We assume the wallbox was replaced, and the new device should use this config.
                // Step 1: update the serial number. Note: this is not persistent as the updated config is not saved.
                qCDebug(dcAmperfied()) << "The serial number of this device is" << serialNumberRead << ". It does not match the serial number in the config, which is"
                                     << serialNumberConfig << ". Updating config with new serial number.";
                thing->setParamValue(energyControlThingSerialNumberParamTypeId, serialNumberRead);

                // Todo: Step 2: search existing things if there is one with this serial number. If yes, that thing should be deleted. Otherwise there
                // will be undefined behaviour when using reconfigure.
            }

            thing->setStateValue(energyControlConnectedStateTypeId, true);
            qCDebug(dcAmperfied()) << "Initialization successful. The wallbox is now reachable. Connected changed to true.";

            // Disabling the auto-standby as it will shut down modbus
            connection->setStandby(AmperfiedEnergyControlModbusRtuConnection::StandbyStandbyDisabled);
        } else {
            qCDebug(dcAmperfied()) << "Initialization failed.";
        }
    });

    connect(connection, &AmperfiedEnergyControlModbusRtuConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            if (connection->version() < 0x0107) {
                qCWarning(dcAmperfied()) << "We require at least version 1.0.7.";
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware 1.0.7."));
                return;
            }
            m_rtuConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding"));
        }
    });

    connect(connection, &AmperfiedEnergyControlModbusRtuConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAmperfied()) << "Updated:" << connection;

        if (connection->chargingCurrent() == 0) {
            thing->setStateValue(energyControlPowerStateTypeId, false);
        } else {
            thing->setStateValue(energyControlPowerStateTypeId, true);
            thing->setStateValue(energyControlMaxChargingCurrentStateTypeId, connection->chargingCurrent() / 10);
        }
        thing->setStateMinMaxValues(energyControlMaxChargingCurrentStateTypeId, connection->minChargingCurrent(), connection->maxChargingCurrent());
        thing->setStateValue(energyControlCurrentPowerStateTypeId, connection->currentPower());
        thing->setStateValue(energyControlTotalEnergyConsumedStateTypeId, connection->totalEnergy() / 1000.0);
        thing->setStateValue(energyControlSessionEnergyStateTypeId, connection->sessionEnergy() / 1000.0);
        switch (connection->chargingState()) {
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateUndefined:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateA1:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateA2:
            thing->setStateValue(energyControlPluggedInStateTypeId, false);
            break;
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateB1:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateB2:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateC1:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateC2:
            thing->setStateValue(energyControlPluggedInStateTypeId, true);
            break;
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateDerating:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateE:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateError:
        case AmperfiedEnergyControlModbusRtuConnection::ChargingStateF:
            qCWarning(dcAmperfied()) << "Unhandled charging state:" << connection->chargingState();
        }

        int phaseCount = 0;
        if (connection->currentL1() > 1) {
            phaseCount++;
        }
        if (connection->currentL2() > 1) {
            phaseCount++;
        }
        if (connection->currentL3() > 1) {
            phaseCount++;
        }
        thing->setStateValue(energyControlChargingStateTypeId, phaseCount > 0);
        if (phaseCount == 0) {
            // Not allowed to set phasecount to 0;
            phaseCount = 1;
        }
        thing->setStateValue(energyControlPhaseCountStateTypeId, phaseCount);
    });

    connection->update();

}

void IntegrationPluginAmperfied::setupTcpConnection(ThingSetupInfo *info)
{
    // This setup method checks the version number of the wallbox before giving the signal that info finished successfully. The version number is read
    // from a modbus register. Modbus calls can fail, making the setup fail. As a result, care needs to be taken to clean up all objects that were created.

    qCDebug(dcAmperfied()) << "setting up TCP connection";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());

    // The parent object given to the connection object should be thing. The point of a parent relation is to delete all children when the parent gets deleted.
    // The connection object should be deleted when the thing gets deleted, to avoid segfault problems.
    AmperfiedModbusTcpConnection *connection = new AmperfiedModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcAmperfied()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    // Use a monitor to detect IP address changes. However, the monitor is not reliable and regularly reports 'not reachable' when the modbus connection
    // is still working. Because of that, check if the modbus connection is working before setting it to disconnect.
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [this, thing, monitor](bool reachable){
        // This may trigger before setup finished, but should only execute after setup. Check m_tcpConnections for thing, because that key won't
        // be in there before setup finished.
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcAmperfied()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (!thing->stateValue(connectHomeConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work. This code should not execute when modbus is still working.
                if (reachable) {
                    m_tcpConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_tcpConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    m_tcpConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(connection, &AmperfiedModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable){
        qCDebug(dcAmperfied()) << "Reachable changed to" << reachable;
        thing->setStateValue(connectHomeConnectedStateTypeId, reachable);
        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(connectHomeCurrentPowerStateTypeId, 0);
            if (m_monitors.contains(thing) && !m_monitors.value(thing)->reachable()) {
                // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                connection->disconnectDevice();
            }
        }
    });

    connect(connection, &AmperfiedModbusTcpConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            qCDebug(dcAmperfied()) << "Initialization finished sucessfully";
            if (connection->version() < 0x0107) {
                qCWarning(dcAmperfied()) << "We require at least version 1.0.7.";
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware 1.0.7."));
                return;
            }
            m_tcpConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding"));
        }
    });

    connect(connection, &AmperfiedModbusTcpConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAmperfied()) << "Updated:" << connection;

        if (connection->chargingCurrent() == 0) {
            thing->setStateValue(connectHomePowerStateTypeId, false);
        } else {
            thing->setStateValue(connectHomePowerStateTypeId, true);
            thing->setStateValue(connectHomeMaxChargingCurrentStateTypeId, connection->chargingCurrent() / 10);
        }
        thing->setStateMinMaxValues(connectHomeMaxChargingCurrentStateTypeId, connection->minChargingCurrent(), connection->maxChargingCurrent());
        thing->setStateValue(connectHomeCurrentPowerStateTypeId, connection->currentPower());
        thing->setStateValue(connectHomeTotalEnergyConsumedStateTypeId, connection->totalEnergy() / 1000.0);
        thing->setStateValue(connectHomeSessionEnergyStateTypeId, connection->sessionEnergy() / 1000.0);
        switch (connection->chargingState()) {
        case AmperfiedModbusTcpConnection::ChargingStateUndefined:
        case AmperfiedModbusTcpConnection::ChargingStateA1:
        case AmperfiedModbusTcpConnection::ChargingStateA2:
            thing->setStateValue(connectHomePluggedInStateTypeId, false);
            break;
        case AmperfiedModbusTcpConnection::ChargingStateB1:
        case AmperfiedModbusTcpConnection::ChargingStateB2:
        case AmperfiedModbusTcpConnection::ChargingStateC1:
        case AmperfiedModbusTcpConnection::ChargingStateC2:
            thing->setStateValue(connectHomePluggedInStateTypeId, true);
            break;
        case AmperfiedModbusTcpConnection::ChargingStateDerating:
        case AmperfiedModbusTcpConnection::ChargingStateE:
        case AmperfiedModbusTcpConnection::ChargingStateError:
        case AmperfiedModbusTcpConnection::ChargingStateF:
            qCWarning(dcAmperfied()) << "Unhandled charging state:" << connection->chargingState();
        }

        int phaseCount = 0;
        if (connection->currentL1() > 1) {
            phaseCount++;
        }
        if (connection->currentL2() > 1) {
            phaseCount++;
        }
        if (connection->currentL3() > 1) {
            phaseCount++;
        }
        thing->setStateValue(connectHomeChargingStateTypeId, phaseCount > 0);
        if (phaseCount == 0) {
            // Not allowed to set phasecount to 0;
            phaseCount = 1;
        }
        thing->setStateValue(connectHomePhaseCountStateTypeId, phaseCount);
    });

    connection->connectDevice();
}


