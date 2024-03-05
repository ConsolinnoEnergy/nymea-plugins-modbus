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

#include "integrationpluginsolaxEvc.h"
#include "plugininfo.h"
#include "discoveryrtu.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>

IntegrationPluginSolaxEvc::IntegrationPluginSolaxEvc()
{

}

void IntegrationPluginSolaxEvc::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == solaxEvcThingClassId) {
        DiscoveryRtu *discovery = new DiscoveryRtu(hardwareManager()->modbusRtuResource(), info);

        connect(discovery, &DiscoveryRtu::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with appropriate settings found. Please set up a modbus RTU master with a baudrate of 9600, 8 data bis, 1 stop bit and no parity first."));
                return;
            }

            qCInfo(dcSolaxEvc()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const DiscoveryRtu::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(solaxEvcThingClassId, result.model, QString("Modbus ID: %1").arg(result.modbusId));

                ParamList params{
                    {solaxEvcThingRtuMasterParamTypeId, result.modbusRtuMasterId},
                    {solaxEvcThingModbusIdParamTypeId, result.modbusId}
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

void IntegrationPluginSolaxEvc::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcSolaxEvc()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == solaxEvcThingClassId) {

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcSolaxEvc()) << "Reconfiguring existing thing" << thing->name();
            m_rtuConnections.take(thing)->deleteLater();
        }

        setupRtuConnection(info);
    }
}

void IntegrationPluginSolaxEvc::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    if (!m_pluginTimer) {
        qCDebug(dcSolaxEvc()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(SolaxEvcModbusRtuConnection *connection, m_rtuConnections) {
                qCDebug(dcSolaxEvc()) << "Updating connection" << connection->modbusRtuMaster() << connection->slaveId();
                connection->update();
            }
        });
        m_pluginTimer->start();
    }
}

void IntegrationPluginSolaxEvc::executeAction(ThingActionInfo *info)
{
    if (info->thing()->thingClassId() == energyControlThingClassId) {
        AmperfiedModbusRtuConnection *connection = m_rtuConnections.value(info->thing());

        if (info->action().actionTypeId() == energyControlPowerActionTypeId) {
            bool power = info->action().paramValue(energyControlPowerActionPowerParamTypeId).toBool();
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? info->thing()->stateValue(energyControlMaxChargingCurrentStateTypeId).toUInt() * 10 : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, power](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(energyControlPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcSolaxEvc()) << "Error setting power:" << reply->error() << reply->errorString();
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
                    qCWarning(dcSolaxEvc()) << "Error setting power:" << reply->error() << reply->errorString();
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
                    qCWarning(dcSolaxEvc()) << "Error setting power:" << reply->error() << reply->errorString();
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
                    qCWarning(dcSolaxEvc()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
    }
}

void IntegrationPluginSolaxEvc::thingRemoved(Thing *thing)
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

void IntegrationPluginSolaxEvc::setupRtuConnection(ThingSetupInfo *info)
{
    // This setup method checks the version number of the wallbox before giving the signal that info finished successfully. The version number is read
    // from a modbus register. Modbus calls can fail, making the setup fail. As a result, care needs to be taken to clean up all objects that were created.

    Thing *thing = info->thing();
    ModbusRtuMaster *master = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(thing->paramValue(energyControlThingRtuMasterParamTypeId).toUuid());
    if (!master) {
        qCWarning(dcSolaxEvc()) << "The Modbus Master is not available any more.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The modbus RTU connection is not available."));
        return;
    }
    quint16 modbusId = thing->paramValue(energyControlThingModbusIdParamTypeId).toUInt();
    AmperfiedModbusRtuConnection *connection = new AmperfiedModbusRtuConnection(master, modbusId, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcSolaxEvc()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
        connection->deleteLater();
    });

    connect(connection, &AmperfiedModbusRtuConnection::reachableChanged, thing, [connection, thing](bool reachable){
        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(energyControlCurrentPowerStateTypeId, 0);
            thing->setStateValue(energyControlConnectedStateTypeId, false);
        }
    });
    connect(connection, &AmperfiedModbusRtuConnection::initializationFinished, thing, [connection, thing](bool success){
        if (success) {
            thing->setStateValue(energyControlConnectedStateTypeId, true);

            // Disabling the auto-standby as it will shut down modbus
            connection->setStandby(AmperfiedModbusRtuConnection::StandbyStandbyDisabled);
        }
    });

    connect(connection, &AmperfiedModbusRtuConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            if (connection->version() < 0x0107) {
                qCWarning(dcSolaxEvc()) << "We require at least version 1.0.7.";
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

    connect(connection, &AmperfiedModbusRtuConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcSolaxEvc()) << "Updated:" << connection;

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
        case AmperfiedModbusRtuConnection::ChargingStateUndefined:
        case AmperfiedModbusRtuConnection::ChargingStateA1:
        case AmperfiedModbusRtuConnection::ChargingStateA2:
            thing->setStateValue(energyControlPluggedInStateTypeId, false);
            break;
        case AmperfiedModbusRtuConnection::ChargingStateB1:
        case AmperfiedModbusRtuConnection::ChargingStateB2:
        case AmperfiedModbusRtuConnection::ChargingStateC1:
        case AmperfiedModbusRtuConnection::ChargingStateC2:
            thing->setStateValue(energyControlPluggedInStateTypeId, true);
            break;
        case AmperfiedModbusRtuConnection::ChargingStateDerating:
        case AmperfiedModbusRtuConnection::ChargingStateE:
        case AmperfiedModbusRtuConnection::ChargingStateError:
        case AmperfiedModbusRtuConnection::ChargingStateF:
            qCWarning(dcSolaxEvc()) << "Unhandled charging state:" << connection->chargingState();
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

void IntegrationPluginSolaxEvc::setupTcpConnection(ThingSetupInfo *info)
{
    // This setup method checks the version number of the wallbox before giving the signal that info finished successfully. The version number is read
    // from a modbus register. Modbus calls can fail, making the setup fail. As a result, care needs to be taken to clean up all objects that were created.

    qCDebug(dcSolaxEvc()) << "setting up TCP connection";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    AmperfiedModbusTcpConnection *connection = new AmperfiedModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcSolaxEvc()) << "Cleaning up ModbusTCP connection because setup has been aborted.";
        connection->disconnectDevice();
        connection->deleteLater();
    });

    // Use a monitor to detect IP address changes. However, the monitor is not reliable and regularly reports 'not reachable' when the modbus connection
    // is still working. Because of that, check if the modbus connection is working before setting it to disconnect.
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [this, thing, monitor](bool reachable){
        // This may trigger before setup finished, but should only execute after setup. Check m_tcpConnections for thing, because that key won't
        // be in there before setup finished.
        if (m_tcpConnections.contains(thing)) {
            qCDebug(dcSolaxEvc()) << "Network device monitor reachable changed for" << thing->name() << reachable;
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
        qCDebug(dcSolaxEvc()) << "Reachable changed to" << reachable;
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
            qCDebug(dcSolaxEvc()) << "Initialization finished sucessfully";
            if (connection->version() < 0x0107) {
                qCWarning(dcSolaxEvc()) << "We require at least version 1.0.7.";
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
        qCDebug(dcSolaxEvc()) << "Updated:" << connection;

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
            qCWarning(dcSolaxEvc()) << "Unhandled charging state:" << connection->chargingState();
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


