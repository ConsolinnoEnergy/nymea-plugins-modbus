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
                qCDebug(dcAbb()) << "Discovery result:" << result.networkDeviceInfo.address().toString() + " (" + result.networkDeviceInfo.macAddress() + ", " + result.networkDeviceInfo.macAddressManufacturer() + ")";

                if (result.firmwareVersion >= MIN_FIRMWARE_VERSION){   
                    qCDebug(dcAbb()) << "Discovery: --> Found Version:" 
                                        << result.firmwareVersion;                      
                }
                else{
                    qCDebug(dcAbb()) << "Discovery: --> Found wrong modbus-device, version-register:" 
                                        << result.firmwareVersion;   
                    qCDebug(dcAbb()) << "Discovery: Minimum required firmware:" 
                                        << MIN_FIRMWARE_VERSION_MAJOR << "."
                                        << MIN_FIRMWARE_VERSION_MINOR << "."
                                        << MIN_FIRMWARE_VERSION_REVISION;   
                }
                
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
    Thing *thing = info->thing();
    if (thing->thingClassId() == TerraRTUThingClassId) {
        ABBModbusRtuConnection *connection = m_rtuConnections.value(thing);

        if (info->action().actionTypeId() == TerraRTUPowerActionTypeId) {
            bool power = info->action().paramValue(TerraRTUPowerActionPowerParamTypeId).toBool();
            // Set charging current if the charging enabled button was pressed
            // 0 if disabled; chosen value if enabled
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? thing->stateValue(TerraRTUMaxChargingCurrentStateTypeId).toUInt(): 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, power](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(TerraRTUPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
            // Toggle charging session if the charging enabled button was pressed
            ModbusRtuReply *replySession = connection->setToggleCharging(power ? 0 : 1);
            connect(replySession, &ModbusRtuReply::finished, info, [info, replySession, power](){
                if (replySession->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(TerraRTUPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << replySession->error() << replySession->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

        if (info->action().actionTypeId() == TerraRTUMaxChargingCurrentActionTypeId) {
            // Set the charging current to the chosen value
            bool power = thing->stateValue(TerraRTUPowerStateTypeId).toBool();
            uint max = info->action().paramValue(TerraRTUMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            if(thing->stateValue(TerraRTUSettableMaxCurrentStateTypeId).toUInt() < max){
                max = thing->stateValue(TerraRTUSettableMaxCurrentStateTypeId).toUInt();
            }
            ModbusRtuReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &ModbusRtuReply::finished, info, [info, reply, max](){
                if (reply->error() == ModbusRtuReply::NoError) {
                    info->thing()->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, max);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting maximum charging current:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

    }

    if (thing->thingClassId() == TerraTCPThingClassId) {
        ABBModbusTcpConnection *connection = m_tcpConnections.value(thing);

        if (info->action().actionTypeId() == TerraTCPPowerActionTypeId) {
            // Set charging current if the charging enabled button was pressed
            // 0 if disabled; chosen value if enabled
            bool power = info->action().paramValue(TerraTCPPowerActionPowerParamTypeId).toBool();
            QModbusReply *reply = connection->setChargingCurrent(power ? thing->stateValue(TerraTCPMaxChargingCurrentStateTypeId).toUInt(): 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(TerraTCPPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
            // Toggle charging session if the charging enabled button was pressed
            QModbusReply *replySession = connection->setToggleCharging(power ? 0 : 1);
            connect(replySession, &QModbusReply::finished, info, [info, replySession, power](){
                if (replySession->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(TerraTCPPowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error setting power:" << replySession->error() << replySession->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }

        if (info->action().actionTypeId() == TerraTCPMaxChargingCurrentActionTypeId) {
            // Set the charging current to the chosen value
            bool power = thing->stateValue(TerraTCPPowerStateTypeId).toBool();
            uint max = info->action().paramValue(TerraTCPMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            if(thing->stateValue(TerraTCPSettableMaxCurrentStateTypeId).toUInt() < max){
                max = thing->stateValue(TerraTCPSettableMaxCurrentStateTypeId).toUInt();
            }
            QModbusReply *reply = connection->setChargingCurrent(power ? max : 0);
            connect(reply, &QModbusReply::finished, info, [info, reply, max](){
                if (reply->error() == QModbusDevice::NoError) {
                    info->thing()->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, max);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcAbb()) << "Error maximum charging current:" << reply->error() << reply->errorString();
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
    qCDebug(dcAbb()) << "setting up RTU connection";
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

        // In case of a temporary disconnect:
        // Make the settings in the app follow the current state of the wallbox
        bool chargingEnabled = thing->stateValue(TerraRTUPowerStateTypeId).toBool();
        thing->setStateValue(TerraRTUPowerStateTypeId, chargingEnabled ? false : true);
        thing->setStateValue(TerraRTUPowerStateTypeId, chargingEnabled);

        quint32 maxCurrent = thing->stateValue(TerraRTUMaxChargingCurrentStateTypeId).toUInt();
        thing->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, maxCurrent == 0 ? 5 : 0);
        thing->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, maxCurrent);
    });

    connect(connection, &ABBModbusRtuConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            qCDebug(dcAbb()) << "FW version " << connection->fwversion();
            if (connection->fwversion() < MIN_FIRMWARE_VERSION) {
                qCWarning(dcAbb()) << "We require at least version "           
                                    << MIN_FIRMWARE_VERSION_MAJOR << "."
                                    << MIN_FIRMWARE_VERSION_MINOR << "."
                                    << MIN_FIRMWARE_VERSION_REVISION;
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware version."
                                + QString::number(MIN_FIRMWARE_VERSION_MAJOR)+"."
                                + QString::number(MIN_FIRMWARE_VERSION_MINOR)+"."
                                + QString::number(MIN_FIRMWARE_VERSION_REVISION)+"."));
                delete connection;
                return;
            }

            info->thing()->setStateValue(TerraRTUFirmwareVersionStateTypeId, QString::number(connection->fwversion()));

            m_rtuConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding."));
        }

        // Upon adding the plugin, make sure the wallbox does not start charging on its own,
        // while charging enabled is set to false
        ModbusRtuReply *reply = connection->setChargingCurrent(0);
        connect(reply, &ModbusRtuReply::finished, reply, &ModbusRtuReply::deleteLater);
        connect(reply, &ModbusRtuReply::finished, info, [info, reply](){
            if (reply->error() == ModbusRtuReply::NoError) {
                info->thing()->setStateValue(TerraRTUPowerStateTypeId, false);
                info->finish(Thing::ThingErrorNoError);
            } else {
                qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
            }
        });
    });

    // Current active power of the wallbox
    connect(connection, &ABBModbusRtuConnection::currentPowerChanged, thing, [thing](float currentPower){
        qCDebug(dcAbb()) << "Wallbox currentPower changed" << currentPower << "W";
        thing->setStateValue(TerraRTUCurrentPowerStateTypeId, currentPower);
    });

    // Session Energy
    connect(connection, &ABBModbusRtuConnection::sessionEnergyChanged, thing, [thing](float sessionEnergy){
        qCDebug(dcAbb()) << "Wallbox sessionEnergy changed" << sessionEnergy / 1000.0 << "kWh";
        if (sessionEnergy != 0) 
        {
            thing->setStateValue(TerraRTUSessionEnergyStateTypeId, sessionEnergy / 1000.0);
        } else {
            float totalEnergy = thing->stateValue(TerraRTUTotalEnergyConsumedStateTypeId).toFloat();
            totalEnergy = totalEnergy + sessionEnergy;
            thing->setStateValue(TerraRTUTotalEnergyConsumedStateTypeId, totalEnergy);
            thing->setStateValue(TerraRTUSessionEnergyStateTypeId, 0);
        }
    });

    // The max current which can be set; Value configured in TerraConfig App
    connect(connection, &ABBModbusRtuConnection::settableMaxCurrentChanged, thing, [thing](quint32 settableMaxCurrent){
        qCDebug(dcAbb()) << "Wallbox settableMaxCurrent changed" << settableMaxCurrent << "A";
        thing->setStateValue(TerraRTUSettableMaxCurrentStateTypeId, settableMaxCurrent);
        thing->setStateMaxValue(TerraRTUMaxChargingCurrentStateTypeId,settableMaxCurrent);
    });

    // Max charging current; Value set in Leaflet App
    connect(connection, &ABBModbusRtuConnection::chargingCurrentLimitChanged, thing, [thing](quint32 chargingCurrentLimit){
        qCDebug(dcAbb()) << "Wallbox chargingCurrentLimit changed" << chargingCurrentLimit<< "A";
        if (chargingCurrentLimit < 6) {
            thing->setStateValue(TerraRTUPowerStateTypeId, false);
        }
        else {
            thing->setStateValue(TerraRTUMaxChargingCurrentStateTypeId, chargingCurrentLimit);
        }        
    });

    // Display error code
    connect(connection, &ABBModbusRtuConnection::errorCodeChanged, thing, [thing](quint32 errorCode){
        qCDebug(dcAbb()) << QString("Wallbox errorCode changed 0x%1").arg(errorCode, 0, 16);
        QMap<int, QString> errorCodeMap = {{0,"Kein Fehler (0x0000)"},{2,"Fehlerstrom erkannt (0x0002)"},{4,"Elektrischer Anschluss (0x0004)"}, \
                                           {8, "Überspannung (0x0008)"}, {16, "Unterspannung (0x0010)"}, {32, "Überstrom (0x0020)"}, {64, "Starker Überstrom (0x0040)"}, \
                                           {128, "Übertemperatur (0x0080)"}, {1024, {"Leistungsrelaisfehler (0x0400)"}}, {2048, "Interner Kommunikationsfehler (0x0800)"}, \
                                           {4096, "E-Verriegelungsstörung (0x1000)"}, {8192, "Fehlende Phase (0x2000)"}};
        thing->setStateValue(TerraRTUErrorCodeStateTypeId, errorCodeMap[errorCode]);
    });

    connect(connection, &ABBModbusRtuConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAbb()) << "Updated:" << connection;

        // Interpret charging state
        switch (connection->chargingState()) {
            case ABBModbusRtuConnection::ChargingStateUndefined:
                qCWarning(dcAbb()) << "Undefined charging state:" << connection->chargingState();
                break;
            case ABBModbusRtuConnection::ChargingStateA0:
            case ABBModbusRtuConnection::ChargingStateA1:
                thing->setStateValue(TerraRTUChargingStateTypeId, false);
                thing->setStateValue(TerraRTUPluggedInStateTypeId, false);
                break;
            case ABBModbusRtuConnection::ChargingStateB1:
            case ABBModbusRtuConnection::ChargingStateB2:
            case ABBModbusRtuConnection::ChargingStateB20:
            case ABBModbusRtuConnection::ChargingStateC1:
                thing->setStateValue(TerraRTUChargingStateTypeId, false);
                thing->setStateValue(TerraRTUPluggedInStateTypeId, true);
                break;
            case ABBModbusRtuConnection::ChargingStateC2:
            case ABBModbusRtuConnection::ChargingStateC20:
                thing->setStateValue(TerraRTUChargingStateTypeId, true);
                thing->setStateValue(TerraRTUPluggedInStateTypeId, true);
                break;
            case ABBModbusRtuConnection::ChargingStateothers:
                qCWarning(dcAbb()) << "Unhandled charging state:" << connection->chargingState();
                break;
            default:
                qCWarning(dcAbb()) << "Invalid charging state:" << connection->chargingState();
                break;
        }

        // Set number of connected phases
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
        thing->setStateValue(TerraRTUPhaseCountStateTypeId, phaseCount > 0 ? phaseCount : 1);
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

    connect(connection, &ABBModbusTcpConnection::initializationFinished, thing, [thing, connection](bool success){
        if (success) {
            thing->setStateValue(TerraTCPConnectedStateTypeId, true);
        }
        
        // In case of a temporary disconnect:
        // Make the settings in the app follow the current state of the wallbox
        bool chargingEnabled = thing->stateValue(TerraTCPPowerStateTypeId).toBool();
        thing->setStateValue(TerraTCPPowerStateTypeId, chargingEnabled ? false : true);
        thing->setStateValue(TerraTCPPowerStateTypeId, chargingEnabled);

        quint32 maxCurrent = thing->stateValue(TerraTCPMaxChargingCurrentStateTypeId).toUInt();
        thing->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, maxCurrent == 0 ? 5 : 0);
        thing->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, maxCurrent);
    });

    connect(connection, &ABBModbusTcpConnection::initializationFinished, info, [this, info, connection](bool success){
        if (success) {
            if (connection->fwversion() < MIN_FIRMWARE_VERSION) {
                qCWarning(dcAbb()) << "We require at least version "           
                                    << MIN_FIRMWARE_VERSION_MAJOR << "."
                                    << MIN_FIRMWARE_VERSION_MINOR << "."
                                    << MIN_FIRMWARE_VERSION_REVISION;
                info->finish(Thing::ThingErrorSetupFailed, QT_TR_NOOP("The firmware of this wallbox is too old. Please update the wallbox to at least firmware version."
                                + QString::number(MIN_FIRMWARE_VERSION_MAJOR)+"."
                                + QString::number(MIN_FIRMWARE_VERSION_MINOR)+"."
                                + QString::number(MIN_FIRMWARE_VERSION_REVISION)+"."));
                delete connection;
                return;
            }

            info->thing()->setStateValue(TerraTCPFirmwareVersionStateTypeId, QString::number(connection->fwversion()));

            m_tcpConnections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The wallbox is not responding."));
        }
        // Upon adding the plugin, make sure the wallbox does not start charging on its own,
        // while charging enabled is set to false
        QModbusReply *reply = connection->setChargingCurrent(0);
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, info, [info, reply](){
            if (reply->error() == QModbusDevice::NoError) {
                info->thing()->setStateValue(TerraTCPPowerStateTypeId, false);
                info->finish(Thing::ThingErrorNoError);
            } else {
                qCWarning(dcAbb()) << "Error setting power:" << reply->error() << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
            }
        });
    });

    // Current active power of the wallbox
    connect(connection, &ABBModbusTcpConnection::currentPowerChanged, thing, [thing](float currentPower){
        qCDebug(dcAbb()) << "Wallbox currentPower changed" << currentPower << "W";
        thing->setStateValue(TerraTCPCurrentPowerStateTypeId, currentPower);
    });

    connect(connection, &ABBModbusTcpConnection::sessionEnergyChanged, thing, [thing](float sessionEnergy){
        qCDebug(dcAbb()) << "Wallbox sessionEnergy changed" << sessionEnergy / 1000.0 << "kWh";
        if (sessionEnergy != 0)
        {
            thing->setStateValue(TerraTCPSessionEnergyStateTypeId, sessionEnergy / 1000.0);
        } else {
            float totalEnergy = thing->stateValue(TerraTCPTotalEnergyConsumedStateTypeId).toFloat();
            totalEnergy = totalEnergy + sessionEnergy;
            thing->setStateValue(TerraTCPTotalEnergyConsumedStateTypeId, totalEnergy);
            thing->setStateValue(TerraTCPSessionEnergyStateTypeId, 0);
        }
    });

    // Session Energy
    connect(connection, &ABBModbusTcpConnection::settableMaxCurrentChanged, thing, [thing](quint32 settableMaxCurrent){
        qCDebug(dcAbb()) << "Wallbox settableMaxCurrent changed" << settableMaxCurrent << "A";
        thing->setStateValue(TerraTCPSettableMaxCurrentStateTypeId, settableMaxCurrent);
        thing->setStateMaxValue(TerraTCPMaxChargingCurrentStateTypeId,settableMaxCurrent);
    });

    // Max charging current; Value set in Leaflet App
    connect(connection, &ABBModbusTcpConnection::chargingCurrentLimitChanged, thing, [thing](quint32 chargingCurrentLimit){
        qCDebug(dcAbb()) << "Wallbox chargingCurrentLimit changed" << chargingCurrentLimit << "A";

        if (chargingCurrentLimit < 6) {
            thing->setStateValue(TerraTCPPowerStateTypeId, false);
        } 
        else {
            thing->setStateValue(TerraTCPMaxChargingCurrentStateTypeId, chargingCurrentLimit);
        }        
    });

    // Display error code
    connect(connection, &ABBModbusTcpConnection::errorCodeChanged, thing, [thing](quint32 errorCode){
        qCDebug(dcAbb()) << QString("Wallbox errorCode changed 0x%1").arg(errorCode, 0, 16);
        QMap<int, QString> errorCodeMap = {{0,"Kein Fehler (0x0000)"},{2,"Fehlerstrom erkannt (0x0002)"},{4,"Elektrischer Anschluss (0x0004)"}, \
                                           {8, "Überspannung (0x0008)"}, {16, "Unterspannung (0x0010)"}, {32, "Überstrom (0x0020)"}, {64, "Starker Überstrom (0x0040)"}, \
                                           {128, "Übertemperatur (0x0080)"}, {1024, {"Leistungsrelaisfehler (0x0400)"}}, {2048, "Interner Kommunikationsfehler (0x0800)"}, \
                                           {4096, "E-Verriegelungsstörung (0x1000)"}, {8192, "Fehlende Phase (0x2000)"}};
        thing->setStateValue(TerraTCPErrorCodeStateTypeId, errorCodeMap[errorCode]);
    });

    connect(connection, &ABBModbusTcpConnection::updateFinished, thing, [connection, thing](){
        qCDebug(dcAbb()) << "Updated:" << connection;
    
        // Interpret charging state
        switch (connection->chargingState()) {
            case ABBModbusTcpConnection::ChargingStateUndefined:
                qCWarning(dcAbb()) << "Undefined charging state:" << connection->chargingState();
                break;
            case ABBModbusTcpConnection::ChargingStateA0:
            case ABBModbusTcpConnection::ChargingStateA1:
                thing->setStateValue(TerraTCPChargingStateTypeId, false);
                thing->setStateValue(TerraTCPPluggedInStateTypeId, false);
                break;
            case ABBModbusTcpConnection::ChargingStateB1:
            case ABBModbusTcpConnection::ChargingStateB2:
            case ABBModbusTcpConnection::ChargingStateB20:
            case ABBModbusTcpConnection::ChargingStateC1:
                thing->setStateValue(TerraTCPChargingStateTypeId, false);
                thing->setStateValue(TerraTCPPluggedInStateTypeId, true);
                break;
            case ABBModbusTcpConnection::ChargingStateC2:
            case ABBModbusTcpConnection::ChargingStateC20:
                thing->setStateValue(TerraTCPChargingStateTypeId, true);
                thing->setStateValue(TerraTCPPluggedInStateTypeId, true);
                break;
            case ABBModbusTcpConnection::ChargingStateothers:
                qCWarning(dcAbb()) << "Unhandled charging state:" << connection->chargingState();
                break;
            default:
                qCWarning(dcAbb()) << "Invalid charging state:" << connection->chargingState();
                break;
        }

        // Set number of connected phases
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
        thing->setStateValue(TerraTCPPhaseCountStateTypeId, phaseCount > 0 ? phaseCount : 1);
        thing->setStateValue(TerraTCPChargingStateTypeId, phaseCount > 0);
    });

    connection->connectDevice();
}


