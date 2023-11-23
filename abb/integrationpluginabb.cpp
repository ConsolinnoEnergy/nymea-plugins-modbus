/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
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

#include "integrationpluginabb.h"
#include "plugininfo.h"
#include "terraRTUdiscovery.h"
#include "terraTCPdiscovery.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include <hardware/modbus/modbusrtuhardwareresource.h>

IntegrationPluginABB::IntegrationPluginABB()
{

}

void IntegrationPluginABB::discoverThings(ThingDiscoveryInfo *info)
{
    hardwareManager()->modbusRtuResource();

    if (info->thingClassId() == TerraRTUThingClassId) {
        TerraRTUDiscovery *discovery = new TerraRTUDiscovery(hardwareManager()->modbusRtuResource(), info);

        connect(discovery, &TerraRTUDiscovery::discoveryFinished, info, [this, info, discovery](bool modbusMasterAvailable){
            if (!modbusMasterAvailable) {
                info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("No modbus RTU master with appropriate settings found. Please set up a modbus RTU master with a baudrate of 19200, 8 data bis, 1 stop bit and even parity first."));
                return;
            }

            qCInfo(dcAbb()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const TerraRTUDiscovery::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(TerraRTUThingClassId, "ABB Terra", QString("Slave ID: %1").arg(result.slaveId));

                ParamList params{
                    {TerraRTUThingRtuMasterParamTypeId, result.modbusRtuMasterId},
                    {TerraRTUThingSlaveIdParamTypeId, result.slaveId}
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

        return;
    }

    if (info->thingClassId() == TerraTCPThingClassId) {
        TerraTCPDiscovery *discovery = new TerraTCPDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &TerraTCPDiscovery::discoveryFinished, info, [this, info, discovery](){
            qCInfo(dcAbb()) << "Discovery results:" << discovery->discoveryResults().count();

            foreach (const TerraTCPDiscovery::Result &result, discovery->discoveryResults()) {
                ThingDescriptor descriptor(TerraTCPThingClassId, "ABB Terra", QString("MAC: %1").arg(result.networkDeviceInfo.macAddress()));

                ParamList params{
                    {TerraTCPThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress()}
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

void IntegrationPluginABB::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAbb()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == TerraRTUThingClassId) {

        if (m_rtuConnections.contains(thing)) {
            qCDebug(dcAbb()) << "Reconfiguring existing thing" << thing->name();
            m_rtuConnections.take(thing)->deleteLater();
        }

        setupRtuConnection(info);
        return;
    }


    if (info->thing()->thingClassId() == TerraTCPThingClassId) {
        if (m_tcpConnections.contains(info->thing())) {
            delete m_tcpConnections.take(info->thing());
        }

        NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
        if (!monitor) {
            monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(MacAddress(thing->paramValue(TerraTCPThingMacAddressParamTypeId).toString()));
            m_monitors.insert(thing, monitor);
        }

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcAbb()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        qCDebug(dcAbb()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(TerraTCPThingMacAddressParamTypeId).toString();
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info](bool reachable){
                qCDebug(dcAbb()) << "Monitor reachable changed!" << reachable;
                if (reachable) {
                    setupTcpConnection(info);
                }
            });
        }
    }
}

void IntegrationPluginABB::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    if (!m_pluginTimer) {
        qCDebug(dcAbb()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(ABBModbusRtuConnection *connection, m_rtuConnections) {
                qCDebug(dcAbb()) << "Updating connection" << connection->modbusRtuMaster() << connection->slaveId();
                connection->update();
            }
            foreach(ABBModbusTcpConnection *connection, m_tcpConnections) {
                qCDebug(dcAbb()) << "Updating connection" << connection->hostAddress();
                connection->update();
            }
        });
        m_pluginTimer->start();
    }
}

void IntegrationPluginABB::executeAction(ThingActionInfo *info)
{
    if (info->thing()->thingClassId() == TerraRTUThingClassId) {
        ABBModbusRtuConnection *connection = m_rtuConnections.value(info->thing());

        if (info->action().actionTypeId() == TerraRTUPowerActionTypeId) {
            bool power = info->action().paramValue(TerraRTUPowerActionPowerParamTypeId).toBool();
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? info->thing()->stateValue(TerraRTUMaxChargingCurrentStateTypeId).toUInt() * 10 : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, power](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(TerraRTUPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
            return;
        }

        if (info->action().actionTypeId() == TerraRTUMaxChargingCurrentActionTypeId) {
            bool power = info->thing()->stateValue(TerraRTUPowerStateTypeId).toBool();
            uint max = info->action().paramValue(TerraRTUMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt() * 10;
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, max](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, max);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

    }

    if (info->thing()->thingClassId() == TerraTCPThingClassId) {
        ABBModbusTcpConnection *connection = m_tcpConnections.value(info->thing());

        if (info->action().actionTypeId() == TerraTCPPowerActionTypeId) {
            bool power = info->action().paramValue(TerraTCPPowerActionPowerParamTypeId).toBool();
            QModbusReply *reply = connection->setChargingCurrent(power ? info->thing()->stateValue(TerraTCPMaxChargingCurrentStateTypeId).toUInt() * 10 : 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(TerraTCPPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

        if (info->action().actionTypeId() == TerraTCPMaxChargingCurrentActionTypeId) {
            bool power = info->thing()->stateValue(TerraTCPPowerStateTypeId).toBool();
            uint max = info->action().paramValue(TerraTCPMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt() * 10;
            QModbusReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, max](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, max);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

    }

}

void IntegrationPluginABB::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == TerraRTUThingClassId) {
        delete m_rtuConnections.take(thing);
    }

    if (thing->thingClassId() == TerraTCPThingClassId) {
        delete m_tcpConnections.take(thing);
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginABB::setupRtuConnection(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    ModbusRtuMaster *master = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(thing->paramValue(TerraRTUThingRtuMasterParamTypeId).toUuid());
    quint16 slaveId = thing->paramValue(TerraRTUThingSlaveIdParamTypeId).toUInt();
    ABBModbusRtuConnection *connection = new ABBModbusRtuConnection(master, slaveId, thing);

    connect(connection, &ABBModbusRtuConnection::reachableChanged, thing, [connection, thing](bool reachable){
        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(TerraRTUCurrentPowerStateTypeId, 0);
            thing->setStateValue(TerraRTUConnectedStateTypeId, false);
        }
    });
    connect(connection, &ABBModbusRtuConnection::initializationFinished, thing, [thing](bool success){
        if (success) {
            thing->setStateValue(TerraRTUConnectedStateTypeId, true);
        }
    });

    connect(connection, &ABBModbusRtuConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            if (connection->fwversion() < MIN_FIRMWARE_VERSION) {
                //TODO: trasform firmware to readable string
                qCWarning(dcAbb()) << "We require at least version " << QString::number(MIN_FIRMWARE_VERSION_MAJOR) << ".xx.xx";
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware "+QString::number(MIN_FIRMWARE_VERSION_MAJOR)+".xx.xx"));
                delete connection;
                return;
            }
            m_rtuConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding"));
        }
    });

    connect(connection, &ABBModbusRtuConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAbb()) << "Updated:" << connection;

        if (connection->chargingCurrentLimit() == 0) {
            thing->setStateValue(TerraRTUPowerStateTypeId, false);
        } else {
            thing->setStateValue(TerraRTUPowerStateTypeId, true);
            thing->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, connection->chargingCurrentLimit() / 10);
        }
        //TODO manage TerraTCPMaxChargingCurrentStateTypeId, as minChargingCurrent and maxChargingCurrent are not available
        // thing->setStateMinMaxValues(TerraRTUMaxChargingCurrentStateTypeId, connection->minChargingCurrent(), connection->maxChargingCurrent());
        thing->setStateValue(TerraRTUCurrentPowerStateTypeId, connection->currentPower());
        thing->setStateValue(TerraRTUSessionEnergyStateTypeId, connection->sessionEnergy() / 1000.0);
        switch (connection->chargingState()) {
            //TODO interpret states
        case ABBModbusRtuConnection::ChargingStateUndefined:
        case ABBModbusRtuConnection::ChargingStateA:
        case ABBModbusRtuConnection::ChargingStateB1:
            thing->setStateValue(TerraRTUPluggedInStateTypeId, false);
            break;
        case ABBModbusRtuConnection::ChargingStateB2:
        case ABBModbusRtuConnection::ChargingStateC1:
        case ABBModbusRtuConnection::ChargingStateC2:
            thing->setStateValue(TerraRTUPluggedInStateTypeId, true);
            break;
        case ABBModbusRtuConnection::ChargingStateothers:
            qCWarning(dcAbb()) << "Unhandled charging state:" << connection->chargingState();
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
        if (phaseCount > 0) {
            thing->setStateValue(TerraRTUPhaseCountStateTypeId, phaseCount);
        }
        thing->setStateValue(TerraRTUChargingStateTypeId, phaseCount > 0);
    });

    connection->update();

}

void IntegrationPluginABB::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcAbb()) << "setting up TCP connection";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    ABBModbusTcpConnection *connection = new ABBModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, info->thing());

    connect(connection, &ABBModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable){
        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(TerraTCPCurrentPowerStateTypeId, 0);
            thing->setStateValue(TerraTCPConnectedStateTypeId, false);
        }
    });


    connect(connection, &ABBModbusTcpConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            if (connection->fwversion() < MIN_FIRMWARE_VERSION) {
                //TODO: trasform firmware to readable string
                qCWarning(dcAbb()) << "We require at least version " << QString::number(MIN_FIRMWARE_VERSION_MAJOR) << ".xx.xx";
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware "+QString::number(MIN_FIRMWARE_VERSION_MAJOR)+".xx.xx"));
                delete connection;
                return;
            }
            m_tcpConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding"));
        }
    });

    connect(connection, &ABBModbusTcpConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAbb()) << "Updated:" << connection;

        thing->setStateValue(TerraTCPConnectedStateTypeId, true);

        if (connection->chargingCurrentLimit() == 0) {
            thing->setStateValue(TerraTCPPowerStateTypeId, false);
        } else {
            thing->setStateValue(TerraTCPPowerStateTypeId, true);
            thing->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, connection->chargingCurrentLimit() / 10);
        }
        //TODO manage TerraTCPMaxChargingCurrentStateTypeId, as minChargingCurrent and maxChargingCurrent are not available
        // thing->setStateMinMaxValues(TerraRTUMaxChargingCurrentStateTypeId, connection->minChargingCurrent(), connection->maxChargingCurrent());
        thing->setStateValue(TerraRTUCurrentPowerStateTypeId, connection->currentPower());
        thing->setStateValue(TerraRTUSessionEnergyStateTypeId, connection->sessionEnergy() / 1000.0);
        switch (connection->chargingState()) {
            //TODO interpret states
        case ABBModbusRtuConnection::ChargingStateUndefined:
        case ABBModbusRtuConnection::ChargingStateA:
        case ABBModbusRtuConnection::ChargingStateB1:
            thing->setStateValue(TerraRTUPluggedInStateTypeId, false);
            break;
        case ABBModbusRtuConnection::ChargingStateB2:
        case ABBModbusRtuConnection::ChargingStateC1:
        case ABBModbusRtuConnection::ChargingStateC2:
            thing->setStateValue(TerraRTUPluggedInStateTypeId, true);
            break;
        case ABBModbusRtuConnection::ChargingStateothers:
            qCWarning(dcAbb()) << "Unhandled charging state:" << connection->chargingState();
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
        if (phaseCount > 0) {
            thing->setStateValue(TerraTCPPhaseCountStateTypeId, phaseCount);
        }
        thing->setStateValue(TerraTCPChargingStateTypeId, phaseCount > 0);
    });

    connection->connectDevice();
}


