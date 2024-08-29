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

#include <sstream>

#include "integrationpluginbroetje.h"
#include "plugininfo.h"
#include "discoveryrtu.h"

IntegrationPluginBroetje::IntegrationPluginBroetje() {}

void IntegrationPluginBroetje::init() {
    connect(hardwareManager()->modbusRtuResource(), &ModbusRtuHardwareResource::modbusRtuMasterRemoved, this, [=] (const QUuid &modbusUuid){
        qCDebug(dcBroetje()) << "Modbus RTU master has been removed" << modbusUuid.toString();

        foreach (Thing *thing, myThings()) {
            if (thing->paramValue(broetjeThingModbusMasterUuidParamTypeId) == modbusUuid) {
                qCWarning(dcBroetje()) << "Modbus RTU hardware resource removed for" << thing << ". The thing will not be functional any more until a new resource has been configured for it.";
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
                {broetjeThingModbusMasterUuidParamTypeId, result.modbusRtuMasterId}
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
        if (m_connections.contains(thing)) {
            qCDebug(dcBroetje()) << "Reconfiguring existing thing" << thing->name();
            m_connections.take(thing)->deleteLater();
        }

        setupConnection(info);
    }
}

void IntegrationPluginBroetje::postSetupThing(Thing *thing) {
    if (thing->thingClassId() == broetjeThingClassId) {
        if (!m_pluginTimer) {
            qCDebug(dcBroetje()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
                foreach (BroetjeModbusRtuConnection *connection, m_connections) {
                    connection->update();
                }
            });
            m_pluginTimer->start();
        }
    }
}

void IntegrationPluginBroetje::thingRemoved(Thing *thing) {

    qCDebug(dcBroetje()) << "Delete thing" << thing->name();

    if (thing->thingClassId() == broetjeThingClassId) {
        if (m_connections.contains(thing))
            m_connections.take(thing)->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginBroetje::executeAction(ThingActionInfo *info) {
    Thing *thing = info->thing();
    if (thing->thingClassId() != broetjeThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }
    BroetjeModbusRtuConnection *connection = m_connections.value(thing);

    if (info->action().actionTypeId() == broetjeSgReadyModeActionTypeId) {
        QString sgReadyModeString = info->action().paramValue(broetjeSgReadyModeActionSgReadyModeParamTypeId).toString();
        qCDebug(dcBroetje()) << "Execute action" << info->action().actionTypeId().toString() << info->action().params();
        BroetjeModbusRtuConnection::SmartGridState sgReadyState;
        if (sgReadyModeString == "Off") {
            sgReadyState = BroetjeModbusRtuConnection::SmartGridStateModeOne;
        } else if (sgReadyModeString == "Low") {
            sgReadyState = BroetjeModbusRtuConnection::SmartGridStateModeTwo;
        } else if (sgReadyModeString == "Standard") {
            sgReadyState = BroetjeModbusRtuConnection::SmartGridStateModeThree;
        } else if (sgReadyModeString == "High") {
            sgReadyState = BroetjeModbusRtuConnection::SmartGridStateModeFour;
        } else {
            qCWarning(dcBroetje()) << "Failed to set SG Ready mode. An unknown SG Ready mode was passed: " << sgReadyModeString;
            info->finish(Thing::ThingErrorHardwareFailure);  // TODO better matching error type?
            return;
        }

        ModbusRtuReply *reply = connection->setSgReadyState(sgReadyState);
        // Note: modbus RTU replies delete them self on finished.
        connect(reply, &ModbusRtuReply::finished, info, [info, reply, sgReadyModeString] {
            if (reply->error() != ModbusRtuReply::NoError) {
                qCWarning(dcBroetje()) << "Set SG ready mode finished with error" << reply->errorString();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            qCDebug(dcBroetje()) << "Execute action finished successfully" << info->action().actionTypeId().toString() << info->action().params();
            info->thing()->setStateValue(broetjeSgReadyModeStateTypeId, sgReadyModeString);
            info->finish(Thing::ThingErrorNoError);
        });
    }
    info->finish(Thing::ThingErrorNoError);
}


void IntegrationPluginBroetje::setupConnection(ThingSetupInfo *info) {

    Thing *thing = info->thing();
    ModbusRtuMaster *master = hardwareManager()->modbusRtuResource()->getModbusRtuMaster(thing->paramValue(broetjeThingModbusMasterUuidParamTypeId).toUuid());
    if (!master) {
        qCWarning(dcBroetje()) << "The Modbus Master is not available any more.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The modbus RTU connection is not available."));
        return;
    }
    quint16 modbusId = thing->paramValue(broetjeThingModbusIdParamTypeId).toUInt();

    // The parent object given to the connection object should be thing. The point of a parent relation is to delete all children when the parent gets deleted.
    // The connection object should be deleted when the thing gets deleted, to avoid segfault problems.
    BroetjeModbusRtuConnection *connection = new BroetjeModbusRtuConnection(master, modbusId, thing);
    connect(info, &ThingSetupInfo::aborted, connection, [=](){
        qCDebug(dcBroetje()) << "Cleaning up ModbusRTU connection because setup has been aborted.";
        connection->deleteLater();
    });

    connect(connection, &BroetjeModbusRtuConnection::reachableChanged, thing, [thing, connection](bool reachable){
        qCDebug(dcBroetje()) << "Reachable changed to" << reachable << "for" << thing;

        if (reachable) {
            connection->initialize();
        } else {
            thing->setStateValue(broetjeConnectedStateTypeId, false);

            // relevant states should be set to 0 here. But the Broetje does not have any relevant states, like the current electric power.
        }
    });

    connect(connection, &BroetjeModbusRtuConnection::initializationFinished, thing, [thing, connection](bool success){
        if (success) {
            thing->setStateValue(broetjeConnectedStateTypeId, true);
            qCDebug(dcBroetje()) << "Initialization successful. The heat pump is now reachable. Connected changed to true.";

            // Read registers function1 and function2. Both need to be set to 10 to send SG-Ready commands. If they are not 10, set them to 10.
            quint16 function1 = connection->function1();
            quint16 function2 = connection->function2();
            qCDebug(dcBroetje()) << "Checking settings: function1 is" << function1 << "and function2 is" << function2;
            if (function1 != 10) {
                qCDebug(dcBroetje()) << "function1 needs to be 10 for SG-Ready to work. Setting function1 to 10.";
                ModbusRtuReply *reply = connection->setFunction1(10);
                // Note: modbus RTU replies delete them self on finished.
                connect(reply, &ModbusRtuReply::finished, [reply] {
                    if (reply->error() != ModbusRtuReply::NoError) {
                        qCWarning(dcBroetje()) << "Set function1 finished with error" << reply->errorString();
                        return;
                    }
                    qCDebug(dcBroetje()) << "Set function1 finished successfully";
                });
            }
            if (function2 != 10) {
                qCDebug(dcBroetje()) << "function2 needs to be 10 for SG-Ready to work. Setting function2 to 10.";
                ModbusRtuReply *reply = connection->setFunction2(10);
                // Note: modbus RTU replies delete them self on finished.
                connect(reply, &ModbusRtuReply::finished, [reply] {
                    if (reply->error() != ModbusRtuReply::NoError) {
                        qCWarning(dcBroetje()) << "Set function2 finished with error" << reply->errorString();
                        return;
                    }
                    qCDebug(dcBroetje()) << "Set function2 finished successfully";
                });
            }
        } else {
            qCDebug(dcBroetje()) << "Initialization failed";
        }
    });

    connect(connection, &BroetjeModbusRtuConnection::initializationFinished, info, [=](bool success){
        if (success) {
            m_connections.insert(info->thing(), connection);
            info->finish(Thing::ThingErrorNoError);
            connection->update();
        } else {
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The heat pump is not responding"));
        }
    });


    connect(connection, &BroetjeModbusRtuConnection::errorChanged, thing, [thing](uint16_t error){
        qCDebug(dcBroetje()) << thing << "Error changed " << error;
        QString errorString;
        if (error == 65535) {
            errorString = "No error";
        } else {
            // Error code should be displayed in hex format.
            std::stringstream ss;
            ss << std::hex << error;
            errorString = QString::fromStdString(ss.str());
        }
        thing->setStateValue(broetjeErrorStateTypeId, errorString);
    });

    connect(connection, &BroetjeModbusRtuConnection::outdoorTemperatureChanged, thing, [thing](float outdoorTemperature){
        qCDebug(dcBroetje()) << thing << "outdoor temperature changed" << outdoorTemperature << "°C";
        thing->setStateValue(broetjeOutdoorTemperatureStateTypeId, outdoorTemperature);
    });

    connect(connection, &BroetjeModbusRtuConnection::flowTemperatureChanged, thing, [this, thing](int flowTemperature){
        qCDebug(dcBroetje()) << thing << "flow temperature changed" << flowTemperature << "°C";
        thing->setStateValue(broetjeFlowTemperatureStateTypeId, flowTemperature);
    });

    connect(connection, &BroetjeModbusRtuConnection::returnTemperatureChanged, thing, [thing](float returnTemperature){
        qCDebug(dcBroetje()) << thing << "return temperature changed" << returnTemperature << "°C";
        thing->setStateValue(broetjeReturnTemperatureStateTypeId, returnTemperature);
    });

    connect(connection, &BroetjeModbusRtuConnection::storageTankTemperatureChanged, thing, [thing](float storageTankTemperature){
        qCDebug(dcBroetje()) << thing << "Storage tank temperature changed" << storageTankTemperature << "°C";
        thing->setStateValue(broetjeStorageTankTemperatureStateTypeId, storageTankTemperature);
    });

    connect(connection, &BroetjeModbusRtuConnection::hotWaterTemperatureChanged, thing, [thing](float hotWaterTemperature){
        qCDebug(dcBroetje()) << thing << "hot water temperature changed" << hotWaterTemperature << "°C";
        thing->setStateValue(broetjeHotWaterTemperatureStateTypeId, hotWaterTemperature);
    });

    connect(connection, &BroetjeModbusRtuConnection::systemStatusChanged, thing, [thing](uint16_t systemStatus){
        QString systemStatusString;
        switch (systemStatus) {
        case 0:
            systemStatusString = "Standby";
            break;
        case 1:
            systemStatusString = "Wärmeanforderung";
            break;
        case 2:
            systemStatusString = "Erzeugerstart";
            break;
        case 3:
            systemStatusString = "Erzeuger HZG";
            break;
        case 4:
            systemStatusString = "Erzeuger TWW";
            break;
        case 5:
            systemStatusString = "Erzeugerstopp";
            break;
        case 6:
            systemStatusString = "Nachlauf Pumpe";
            break;
        case 7:
            systemStatusString = "Kühlbetrieb";
            break;
        case 8:
            systemStatusString = "Regelabschaltung";
            break;
        case 9:
            systemStatusString = "Startverhinderung";
            break;
        case 10:
            systemStatusString = "Verriegelungsmodus";
            break;
        default:
            systemStatusString = QString("%1").arg(systemStatus);
        }

        qCDebug(dcBroetje()) << thing << "System status changed " << systemStatusString;
        thing->setStateValue(broetjeSystemStatusStateTypeId, systemStatusString);
    });

    connect(connection, &BroetjeModbusRtuConnection::subsystemStatusChanged, thing, [thing](uint16_t subsystemStatus){
        QString subsystemStatusString;
        bool sgReadyActive{false};
        switch (subsystemStatus) {
        case 0:
            subsystemStatusString = "Standby";
            break;
        case 1:
            subsystemStatusString = "Pausenzeit";
            break;
        case 3:
            subsystemStatusString = "Stop Pumpe";
            break;
        case 4:
            subsystemStatusString = "Warte auf Startfreigabe";
            break;
        case 89:
            subsystemStatusString = "BL WP aus";
            break;
        case 94:
            subsystemStatusString = "SG-Steuerlogik aktiv";
            sgReadyActive = true;
            break;
        default:
            subsystemStatusString = QString("%1").arg(subsystemStatus);
        }

        qCDebug(dcBroetje()) << thing << "Subsystem status changed " << subsystemStatusString;
        thing->setStateValue(broetjeSubsystemStatusStateTypeId, subsystemStatusString);
        thing->setStateValue(broetjeSgReadyActiveStateTypeId, sgReadyActive);
    });

    connect(connection, &BroetjeModbusRtuConnection::heatingPowerChanged, thing, [thing](float heatingPower){
        qCDebug(dcBroetje()) << thing << "Heating power changed" << heatingPower << "kW";
        thing->setStateValue(broetjeHeatingPowerStateTypeId, heatingPower);
    });

    connect(connection, &BroetjeModbusRtuConnection::sgReadyStateChanged, thing, [thing](BroetjeModbusRtuConnection::SmartGridState smartGridState){
        qCDebug(dcBroetje()) << thing << "SG Ready mode changed" << smartGridState;
        switch (smartGridState) {
        case BroetjeModbusRtuConnection::SmartGridStateModeOne:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Off");
            break;
        case BroetjeModbusRtuConnection::SmartGridStateModeTwo:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Low");
            break;
        case BroetjeModbusRtuConnection::SmartGridStateModeThree:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "Standard");
            break;
        case BroetjeModbusRtuConnection::SmartGridStateModeFour:
            thing->setStateValue(broetjeSgReadyModeStateTypeId, "High");
            break;
        }
    });

    connection->update();
}
