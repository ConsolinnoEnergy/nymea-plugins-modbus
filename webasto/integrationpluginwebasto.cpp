/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#include "integrationpluginwebasto.h"
#include "webastodiscovery.h"
#include "plugininfo.h"

#include <types/param.h>
#include <hardware/electricity.h>
#include <network/networkdevicediscovery.h>
#include <network/networkaccessmanager.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "../vestel/evc04discovery.h"

// Known problems: For the Webasto Next, the registers 5004 (chargePower) and 5006 (chargingAction) are read only. But the current Nymea version (1.7) is buggy for
// read only registers (compile error), so the workaround is to set them to RW. This will constantly give errors when the plugin tries to read them (which is not
// possible, hence the error), but other than that it works.
// Another thing for the Next: When the wallbox is power cycled, it takes a really long time before it starts answering modbus calls again (~5 min). The plugin will
// recover the connected state, but it takes unusually long.

IntegrationPluginWebasto::IntegrationPluginWebasto()
{
}

void IntegrationPluginWebasto::init()
{

}

void IntegrationPluginWebasto::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcWebasto()) << "Failed to discover network devices. The network device discovery is not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available."));
        return;
    }

    if (info->thingClassId() == webastoNextThingClassId) {

        qCInfo(dcWebasto()) << "Start discovering Webasto NEXT in the local network...";
        m_nextDiscoveryRunning = true;
        foreach(WebastoNextModbusTcpConnection *connection, m_webastoNextConnections) {
            // Reconfigure won't work without this. The device will not answer modbus calls (discovery will skip it) if there already is another active connection.
            qCWarning(dcWebasto()) << "Disconnecting existing device" << connection->hostAddress().toString()
                                   << "temporarily to not interfere with discovery. Nothing to worry about, the device will reconnect after discovery is done.";
            connection->disconnectDevice();
        }


        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        WebastoDiscovery *discovery = new WebastoDiscovery(hardwareManager()->networkDeviceDiscovery(), info);
        connect(discovery, &WebastoDiscovery::discoveryFinished, info, [=](){
            foreach (const WebastoDiscovery::Result &result, discovery->results()) {

                QString title = "Webasto Next";
                if (!result.networkDeviceInfo.hostName().isEmpty()){
                    title.append(" (" + result.networkDeviceInfo.hostName() + ")");
                }

                QString description = result.networkDeviceInfo.address().toString();
                if (result.networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                    description += " " + result.networkDeviceInfo.macAddress();
                } else {
                    description += " " + result.networkDeviceInfo.macAddress() + " (" + result.networkDeviceInfo.macAddressManufacturer() + ")";
                }

                ThingDescriptor descriptor(webastoNextThingClassId, title, description);
                qCDebug(dcWebasto()) << "Discovered:" << descriptor.title() << descriptor.description();

                ParamList params;
                params << Param(webastoNextThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcWebasto()) << "A configuration already exists for the discovered" << title;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcWebasto()) << "Found new" << title;
                }


                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
            m_nextDiscoveryRunning = false;
            foreach(WebastoNextModbusTcpConnection *connection, m_webastoNextConnections) {
                qCDebug(dcWebasto()) << "Discovery is done, reconnecting existing device" << connection->hostAddress().toString();
                connection->reconnectDevice();

                // Warning: This code needs to execute, otherwise the plugin is effectively disabled.
                // Research needed: Is it guaranteed, that this code will trigger? Is a different code path possible, for example when the discovery fails?
            }
        });

        discovery->startDiscovery();
        return;
    }

    if (info->thingClassId() == webastoUniteThingClassId ||
        info->thingClassId() == vestelEVC04ThingClassId ||
        info->thingClassId() == eonDriveThingClassId) {

        qCInfo(dcWebasto()) << "Start discovering Webasto Unite | Vestel EVC04 | EON Drive in the local network...";
        m_uniteDiscoveryRunning = true;
        foreach(EVC04ModbusTcpConnection *connection, m_evc04Connections) {
            // Reconfigure won't work without this. The device will not answer modbus calls (discovery will skip it) if there already is another active connection.
            qCWarning(dcWebasto()) << "Disconnecting existing device" << connection->hostAddress().toString()
                                   << "temporarily to not interfere with discovery. Nothing to worry about, the device will reconnect after discovery is done.";
            connection->disconnectDevice();
        }

        // Create a discovery with the info as parent for auto deleting the object once the discovery info is done
        EVC04Discovery *discovery = new EVC04Discovery(hardwareManager()->networkDeviceDiscovery(), dcWebasto(), info);
        connect(discovery, &EVC04Discovery::discoveryFinished, info, [=](){
            foreach (const EVC04Discovery::Result &result, discovery->discoveryResults()) {

                if (result.brand != "Webasto") {
                    qCDebug(dcWebasto()) << "Brand is" << result.brand;
                    //qCDebug(dcWebasto()) << "Skipping Vestel wallbox without Webasto branding...";
                    //continue;
                }
                QString title = result.brand + " " + result.model;
                QString description = result.chargepointId;
                ThingDescriptor descriptor(info->thingClassId(), title, description);
                qCDebug(dcWebasto()) << "Discovered:" << descriptor.title() << descriptor.description();

                ParamList params;
                if (info->thingClassId() == webastoUniteThingClassId) {
                    params << Param(webastoUniteThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                } else if (info->thingClassId() == vestelEVC04ThingClassId) {
                    params << Param(vestelEVC04ThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                } else if (info->thingClassId() == eonDriveThingClassId) {
                    params << Param(eonDriveThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                }
                descriptor.setParams(params);

                // Check if this device has already been configured. If yes, take it's ThingId. This does two things:
                // - During normal configure, the discovery won't display devices that have a ThingId that already exists. So this prevents a device from beeing added twice.
                // - During reconfigure, the discovery only displays devices that have a ThingId that already exists. For reconfigure to work, we need to set an already existing ThingId.
                Thing *existingThing = myThings().findByParams(descriptor.params());
                if (existingThing) {
                    qCDebug(dcWebasto()) << "A configuration already exists for the discovered" << title;
                    descriptor.setThingId(existingThing->id());
                } else {
                    qCDebug(dcWebasto()) << "Found new" << title;
                }

                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
            m_uniteDiscoveryRunning = false;
            foreach(EVC04ModbusTcpConnection *connection, m_evc04Connections) {
                qCDebug(dcWebasto()) << "Discovery is done, reconnecting existing device" << connection->hostAddress().toString();
                connection->reconnectDevice();

                // Warning: This code needs to execute, otherwise the plugin is effectively disabled.
                // Research needed: Is it guaranteed, that this code will trigger? Is a different code path possible, for example when the discovery fails?
            }
        });
        discovery->startDiscovery();

    }
    Q_ASSERT_X(false, "discoverThings", QString("Unhandled thingClassId: %1").arg(info->thingClassId().toString()).toUtf8());
}

void IntegrationPluginWebasto::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcWebasto()) << "Setup thing" << thing->name();

    if (thing->thingClassId() == webastoNextThingClassId) {
        m_nextSetupRunning = false;

        // Handle reconfigure
        if (m_webastoNextConnections.contains(thing)) {
            qCDebug(dcWebasto()) << "Reconfiguring existing thing" << thing->name();
            m_webastoNextConnections.take(thing)->deleteLater();
        }
        if (m_monitors.contains(thing)) {
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }

        MacAddress macAddress = MacAddress(thing->paramValue(webastoNextThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcWebasto()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the thing."));
            return;
        }

        // Create the monitor
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);

        QHostAddress address = monitor->networkDeviceInfo().address();
        if (address.isNull()) {
            qCWarning(dcWebasto()) << "Cannot set up thing. The host address is not known yet. Maybe it will be available in the next run...";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The host address is not known yet. Trying later again."));
            return;
        }

        // Clean up in case the setup gets aborted
        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcWebasto()) << "Unregister monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // During Nymea startup, it can happen that the monitor is not yet ready when this code triggers. Not ready means, the monitor has not yet scanned the network and
        // does not yet have a mapping of mac<->IP. Because of this, the monitor can't give an IP address when asked for, and setup would fail. To circumvent this problem,
        // wait for the monitor to signal "reachable". When the monitor emits this signal, it is ready to give an IP address.
        if (monitor->reachable()) {
            setupWebastoNextConnection(info);
        } else {
            qCDebug(dcWebasto()) << "Waiting for the network monitor to get reachable before continue to set up the connection" << thing->name() << address.toString() << "...";
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                if (reachable && !m_nextSetupRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_nextSetupRunning = true;
                    qCDebug(dcWebasto()) << "The monitor for thing setup" << thing->name() << "is now reachable. Continue setup...";
                    setupWebastoNextConnection(info);
                }
            });
        }

        return;
    }

    if (thing->thingClassId() == webastoUniteThingClassId ||
        thing->thingClassId() == vestelEVC04ThingClassId ||
        thing->thingClassId() == eonDriveThingClassId) {
        m_uniteSetupRunning = false;

        if (m_evc04Connections.contains(thing)) {
            qCDebug(dcWebasto()) << "Reconfiguring existing thing" << thing->name();
            m_evc04Connections.take(thing)->deleteLater();
        }
        if (m_monitors.contains(thing)) {
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }

        MacAddress macAddress = MacAddress(thing->paramValue("macAddress").toString());
        if (macAddress.isNull()) {
            qCWarning(dcWebasto()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the thing."));
            return;
        }

        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcWebasto()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        // During Nymea startup, it can happen that the monitor is not yet ready when this code triggers. Not ready means, the monitor has not yet scanned the network and
        // does not yet have a mapping of mac<->IP. Because of this, the monitor can't give an IP address when asked for, and setup would fail. To circumvent this problem,
        // wait for the monitor to signal "reachable". When the monitor emits this signal, it is ready to give an IP address.
        if (monitor->reachable()) {
            setupEVC04Connection(info);
        } else {
            qCDebug(dcWebasto()) << "Waiting for the network monitor to get reachable before continuing to set up the connection" << thing->name() << "...";
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                if (reachable && !m_uniteSetupRunning) {
                    // The monitor is unreliable and can change reachable true->false->true before setup is done. Make sure this runs only once.
                    m_uniteSetupRunning = true;
                    qCDebug(dcWebasto()) << "The monitor for thing setup" << thing->name() << "is now reachable. Continuing setup on" << monitor->networkDeviceInfo().address().toString();
                    setupEVC04Connection(info);
                }
            });
        }

        return;
    }

    Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());

}

void IntegrationPluginWebasto::postSetupThing(Thing *thing)
{
    qCDebug(dcWebasto()) << "Post setup thing" << thing->name();
    if (!m_pluginTimer) {
        qCDebug(dcWebasto()) << "Setting up refresh timer for Webasto connections";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(1);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {

            if (!m_nextDiscoveryRunning) {
                foreach(WebastoNextModbusTcpConnection *connection, m_webastoNextConnections) {
                    if (connection->reachable()) {
                        connection->update();
                    }
                }
            }

            if (!m_uniteDiscoveryRunning) {
                foreach(EVC04ModbusTcpConnection *connection, m_evc04Connections) {
                    if (connection->reachable()) {
                        qCDebug(dcWebasto()) << "Updating connection" << connection->hostAddress().toString();
                        connection->update();
                        QModbusReply *reply = connection->setAliveRegister(1);
                        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                    }
                }
            }

        });

        m_pluginTimer->start();
    }

    if (thing->thingClassId() == webastoNextThingClassId) {
        if (m_webastoNextConnections.contains(thing)) {
            WebastoNextModbusTcpConnection *connection = m_webastoNextConnections.value(thing);
            if (connection->reachable()) {
                thing->setStateValue(webastoNextConnectedStateTypeId, true);
                connection->update();
            } else {
                // We start the connection mechanism only if the monitor says the thing is reachable
                if (m_monitors.value(thing)->reachable()) {
                    connection->connectDevice();
                }
            }
            return;
        }
    }
}

void IntegrationPluginWebasto::thingRemoved(Thing *thing)
{
    qCDebug(dcWebasto()) << "Delete thing" << thing->name();

    if (thing->thingClassId() == webastoNextThingClassId && m_webastoNextConnections.contains(thing)) {
        WebastoNextModbusTcpConnection *connection = m_webastoNextConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if ((thing->thingClassId() == webastoUniteThingClassId ||
         thing->thingClassId() == vestelEVC04ThingClassId ||
         thing->thingClassId() == eonDriveThingClassId) && 
         m_evc04Connections.contains(thing)) {

        EVC04ModbusTcpConnection *connection = m_evc04Connections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    // Unregister related hardware resources
    if (m_monitors.contains(thing))
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));

    if (m_timeoutCount.contains(thing))
        m_timeoutCount.remove(thing);

    if (m_pluginTimer && myThings().isEmpty()) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}


void IntegrationPluginWebasto::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() == webastoNextThingClassId) {

        WebastoNextModbusTcpConnection *connection = m_webastoNextConnections.value(thing);
        if (!connection) {
            qCWarning(dcWebasto()) << "Can't find modbus connection for" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!connection->reachable()) {
            qCWarning(dcWebasto()) << "Cannot execute action because the connection of" << thing << "is not reachable.";
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The charging station is not reachable."));
            return;
        }

        if (action.actionTypeId() == webastoNextPowerActionTypeId) {
            bool power = action.paramValue(webastoNextPowerActionPowerParamTypeId).toBool();

            // If this action was executed by the user, we start a new session, otherwise we assume it was a some charging logic
            // and we keep the current session.

            if (power && action.triggeredBy() == Action::TriggeredByUser) {
                // First send 0 ChargingActionNoAction before sending 1 start session
                qCDebug(dcWebasto()) << "Enable charging action triggered by user. Restarting the session.";
                QModbusReply *reply = connection->setChargingAction(WebastoNextModbusTcpConnection::ChargingActionNoAction);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, info, [this, info, reply, power](){
                    if (reply->error() == QModbusDevice::NoError) {
                        info->thing()->setStateValue(webastoNextPowerStateTypeId, power);
                        qCDebug(dcWebasto()) << "Restart charging session request finished successfully.";
                        info->finish(Thing::ThingErrorNoError);
                    } else {
                        qCWarning(dcWebasto()) << "Restart charging session request finished with error:" << reply->errorString();
                        info->finish(Thing::ThingErrorHardwareFailure);
                    }

                    // Note: even if "NoAction" failed, we try to send the start charging action and report the error there just in case
                    executeWebastoNextPowerAction(info, power);
                });
            } else {
                executeWebastoNextPowerAction(info, power);
            }
        } else if (action.actionTypeId() == webastoNextMaxChargingCurrentActionTypeId) {
            quint16 chargingCurrent = action.paramValue(webastoNextMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            qCDebug(dcWebasto()) << "Set max charging current of" << thing << "to" << chargingCurrent << "ampere";
            QModbusReply *reply = connection->setChargeCurrent(chargingCurrent);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, chargingCurrent](){
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcWebasto()) << "Set max charging current finished successfully.";
                    info->thing()->setStateValue(webastoNextMaxChargingCurrentStateTypeId, chargingCurrent);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcWebasto()) << "Set max charging current request finished with error:" << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });

        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
        }

        return;
    }

    if (info->thing()->thingClassId() == webastoUniteThingClassId) {
        EVC04ModbusTcpConnection *evc04Connection = m_evc04Connections.value(info->thing());

        if (info->action().actionTypeId() == webastoUnitePowerActionTypeId) {
            bool power = info->action().paramValue(webastoUnitePowerActionPowerParamTypeId).toBool();

            // If the car is *not* connected, writing a 0 to the charging current register will cause it to go to 6 A instead of 0
            // Because of this, we we're not connected, we'll do nothing, but once it get's connected, we'll sync the state over (see below in cableStateChanged)
            if (!power && evc04Connection->cableState() < EVC04ModbusTcpConnection::CableStateCableConnectedVehicleConnected) {
                info->thing()->setStateValue(webastoUnitePowerStateTypeId, false);
                info->finish(Thing::ThingErrorNoError);
                return;
            }

            QModbusReply *reply = evc04Connection->setChargingCurrent(power ? info->thing()->stateValue(webastoUniteMaxChargingCurrentStateTypeId).toUInt() : 0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    if (!power) {
                        qCDebug(dcWebasto()) << "Turning off wallbox by setting maxChargingCurrent to 0 finished successfully.";
                    } else {
                        qCDebug(dcWebasto()) << "Turning on wallbox by setting maxChargingCurrent to" << info->thing()->stateValue(webastoUniteMaxChargingCurrentStateTypeId).toUInt() << "finished successfully.";
                    }
                    info->thing()->setStateValue(webastoUnitePowerStateTypeId, power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
        if (info->action().actionTypeId() == webastoUniteMaxChargingCurrentActionTypeId) {

            // Setting a charge current will turn on the wallbox. So only send a charge current when the wallbox should be on. Otherwise, just write the charge current
            // to the state. The current will be set when the power action is triggered.
            bool power = info->thing()->stateValue(webastoUnitePowerStateTypeId).toBool();
            int maxChargingCurrent = info->action().paramValue(webastoUniteMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toInt();
            if (power) {
                QModbusReply *reply = evc04Connection->setChargingCurrent(maxChargingCurrent);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, info, [info, reply, maxChargingCurrent](){
                    if (reply->error() == QModbusDevice::NoError) {
                        qCDebug(dcWebasto()) << "Setting max charging current finished successfully.";
                        info->thing()->setStateValue(webastoUniteMaxChargingCurrentStateTypeId, maxChargingCurrent);
                        info->finish(Thing::ThingErrorNoError);
                    } else {
                        qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                        info->finish(Thing::ThingErrorHardwareFailure);
                    }
                });
            } else {
                qCDebug(dcWebasto()) << "Setting max charging current registered, but not sending modbus call because the wallbox should be off.";
                info->thing()->setStateValue(webastoUniteMaxChargingCurrentStateTypeId, maxChargingCurrent);
                info->finish(Thing::ThingErrorNoError);
            }
        }
        return;
    }

    if (info->thing()->thingClassId() == vestelEVC04ThingClassId) {
        EVC04ModbusTcpConnection *evc04Connection = m_evc04Connections.value(info->thing());

        if (info->action().actionTypeId() == vestelEVC04PowerActionTypeId) {
            bool power = info->action().paramValue(vestelEVC04PowerActionPowerParamTypeId).toBool();

            // If the car is *not* connected, writing a 0 to the charging current register will cause it to go to 6 A instead of 0
            // Because of this, we we're not connected, we'll do nothing, but once it get's connected, we'll sync the state over (see below in cableStateChanged)
            if (!power && evc04Connection->cableState() < EVC04ModbusTcpConnection::CableStateCableConnectedVehicleConnected) {
                info->thing()->setStateValue("power", false);
                info->finish(Thing::ThingErrorNoError);
                return;
            }

            QModbusReply *reply = evc04Connection->setChargingCurrent(power ? info->thing()->stateValue("maxChargingCurrent").toUInt() : 0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    if (!power) {
                        qCDebug(dcWebasto()) << "Turning off wallbox by setting maxChargingCurrent to 0 finished successfully.";
                    } else {
                        qCDebug(dcWebasto()) << "Turning on wallbox by setting maxChargingCurrent to" << info->thing()->stateValue("maxChargingCurrent").toUInt() << "finished successfully.";
                    }
                    info->thing()->setStateValue("power", power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
        if (info->action().actionTypeId() == vestelEVC04MaxChargingCurrentActionTypeId) {

            // Setting a charge current will turn on the wallbox. So only send a charge current when the wallbox should be on. Otherwise, just write the charge current
            // to the state. The current will be set when the power action is triggered.
            bool power = info->thing()->stateValue("power").toBool();
            int maxChargingCurrent = info->action().paramValue(vestelEVC04MaxChargingCurrentActionMaxChargingCurrentParamTypeId).toInt();
            if (power) {
                QModbusReply *reply = evc04Connection->setChargingCurrent(maxChargingCurrent);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, info, [info, reply, maxChargingCurrent](){
                    if (reply->error() == QModbusDevice::NoError) {
                        qCDebug(dcWebasto()) << "Setting max charging current finished successfully.";
                        info->thing()->setStateValue("maxChargingCurrent", maxChargingCurrent);
                        info->finish(Thing::ThingErrorNoError);
                    } else {
                        qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                        info->finish(Thing::ThingErrorHardwareFailure);
                    }
                });
            } else {
                qCDebug(dcWebasto()) << "Setting max charging current registered, but not sending modbus call because the wallbox should be off.";
                info->thing()->setStateValue("maxChargingCurrent", maxChargingCurrent);
                info->finish(Thing::ThingErrorNoError);
            }
        }
        return;
    }

    if (info->thing()->thingClassId() == eonDriveThingClassId) {
        EVC04ModbusTcpConnection *evc04Connection = m_evc04Connections.value(info->thing());

        if (info->action().actionTypeId() == eonDrivePowerActionTypeId) {
            bool power = info->action().paramValue(eonDrivePowerActionPowerParamTypeId).toBool();

            // If the car is *not* connected, writing a 0 to the charging current register will cause it to go to 6 A instead of 0
            // Because of this, we we're not connected, we'll do nothing, but once it get's connected, we'll sync the state over (see below in cableStateChanged)
            if (!power && evc04Connection->cableState() < EVC04ModbusTcpConnection::CableStateCableConnectedVehicleConnected) {
                info->thing()->setStateValue("power", false);
                info->finish(Thing::ThingErrorNoError);
                return;
            }

            QModbusReply *reply = evc04Connection->setChargingCurrent(power ? info->thing()->stateValue("maxChargingCurrent").toUInt() : 0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, info, [info, reply, power](){
                if (reply->error() == QModbusDevice::NoError) {
                    if (!power) {
                        qCDebug(dcWebasto()) << "Turning off wallbox by setting maxChargingCurrent to 0 finished successfully.";
                    } else {
                        qCDebug(dcWebasto()) << "Turning on wallbox by setting maxChargingCurrent to" << info->thing()->stateValue("maxChargingCurrent").toUInt() << "finished successfully.";
                    }
                    info->thing()->setStateValue("power", power);
                    info->finish(Thing::ThingErrorNoError);
                } else {
                    qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                    info->finish(Thing::ThingErrorHardwareFailure);
                }
            });
        }
        if (info->action().actionTypeId() == eonDriveMaxChargingCurrentActionTypeId) {

            // Setting a charge current will turn on the wallbox. So only send a charge current when the wallbox should be on. Otherwise, just write the charge current
            // to the state. The current will be set when the power action is triggered.
            bool power = info->thing()->stateValue("power").toBool();
            int maxChargingCurrent = info->action().paramValue(eonDriveMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toInt();
            if (power) {
                QModbusReply *reply = evc04Connection->setChargingCurrent(maxChargingCurrent);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, info, [info, reply, maxChargingCurrent](){
                    if (reply->error() == QModbusDevice::NoError) {
                        qCDebug(dcWebasto()) << "Setting max charging current finished successfully.";
                        info->thing()->setStateValue("maxChargingCurrent", maxChargingCurrent);
                        info->finish(Thing::ThingErrorNoError);
                    } else {
                        qCWarning(dcWebasto()) << "Error setting maxChargingCurrent:" << reply->error() << reply->errorString();
                        info->finish(Thing::ThingErrorHardwareFailure);
                    }
                });
            } else {
                qCDebug(dcWebasto()) << "Setting max charging current registered, but not sending modbus call because the wallbox should be off.";
                info->thing()->setStateValue("maxChargingCurrent", maxChargingCurrent);
                info->finish(Thing::ThingErrorNoError);
            }
        }
        return;
    }

    Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
}


void IntegrationPluginWebasto::setupWebastoNextConnection(ThingSetupInfo *info)
{
    Thing *thing = info->thing();

    QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();
    uint port = 502;
    quint16 slaveId = 1;

    qCDebug(dcWebasto()) << "Setting up webasto next connection on" << QString("%1:%2").arg(address.toString()).arg(port) << "slave ID:" << slaveId;
    WebastoNextModbusTcpConnection *webastoNextConnection = new WebastoNextModbusTcpConnection(address, port, slaveId, this);
    webastoNextConnection->setTimeout(500);
    webastoNextConnection->setNumberOfRetries(3);
    m_webastoNextConnections.insert(thing, webastoNextConnection);
    connect(info, &ThingSetupInfo::aborted, webastoNextConnection, [=](){
        webastoNextConnection->deleteLater();
        m_webastoNextConnections.remove(thing);
    });

    // Reconnect on monitor reachable changed
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "is now reachable at" << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_webastoNextConnections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue(webastoNextConnectedStateTypeId).toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcWebasto()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_webastoNextConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_webastoNextConnections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcWebasto()) << "The device is offline. Stopping communication attempts.";
                    m_webastoNextConnections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(webastoNextConnection, &WebastoNextModbusTcpConnection::reachableChanged, thing, [this, thing, webastoNextConnection, monitor](bool reachable){
        if (m_nextDiscoveryRunning == true) {
            // Don't do anything if this triggers because of a discovery.
            return;
        }

        qCDebug(dcWebasto()) << "Reachable changed to" << reachable << "for" << thing << ". Monitor is" << monitor->reachable();

        thing->setStateValue(webastoNextConnectedStateTypeId, reachable);
        if (reachable) {
            webastoNextConnection->update();
        } else {            
            thing->setStateValue(webastoNextCurrentPowerStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPowerPhaseAStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPowerPhaseBStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPowerPhaseCStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPhaseAStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPhaseBStateTypeId, 0);
            thing->setStateValue(webastoNextCurrentPhaseCStateTypeId, 0);

            // Check the monitor. If the monitor is reachable, get the current IP (maybe it changed) and reconnect.
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_webastoNextConnections.contains(thing)) {
                    qCDebug(dcWebasto()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_webastoNextConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_webastoNextConnections.value(thing)->reconnectDevice();
                }
            }
        }
    });

    connect(webastoNextConnection, &WebastoNextModbusTcpConnection::checkReachabilityFailed, thing, [this, monitor, thing](){
        // Get the connection from the list, to avoid problems when the device is beeing deleted. If it is in the process of beeing deleted, the connection will not be in the list.
        if (m_webastoNextConnections.contains(thing)) {
            // If the reachability failed, try again 10 seconds later. This is needed to reconnect to the device after it was turned off.
            // After startup, apparently the ModbusMaster can connecte to the device long before it will actually answer a modbus call. The
            // modbus call will return an error and the reachability will fail. After a while the device will answer modbus calls, so just try
            // again a bit later.
            qCDebug(dcWebasto()) << "Failed communication attempt with" << thing << ". Trying again in 10 seconds.";
            QTimer::singleShot(10000, thing, [this, monitor, thing](){
                // Again, check the list for the connection object to not reconnect a device that is beeing removed.
                if (m_webastoNextConnections.contains(thing) && monitor->reachable() && !m_webastoNextConnections.value(thing)->reachable()) {
                    qCDebug(dcWebasto()) << "Next communication attempt with" << thing << ", trying IP address" << monitor->networkDeviceInfo().address();
                    m_webastoNextConnections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_webastoNextConnections.value(thing)->reconnectDevice();
                }
            });
        }
    });

    connect(webastoNextConnection, &WebastoNextModbusTcpConnection::updateFinished, thing, [thing, webastoNextConnection](){

        if (!webastoNextConnection->connected()) {
            qCDebug(dcWebasto()) << "Skipping Webasto Next updateFinished, device is not connected.";
            return;
        }

        //qCDebug(dcWebasto()) << "Update finished" << webastoNextConnection;
        qCDebug(dcWebasto()) << "Update finished";
        // States
        switch (webastoNextConnection->chargeState()) {
        case WebastoNextModbusTcpConnection::ChargeStateIdle:
            thing->setStateValue(webastoNextChargingStateTypeId, false);
            break;
        case WebastoNextModbusTcpConnection::ChargeStateCharging:
            thing->setStateValue(webastoNextChargingStateTypeId, true);

            // The wallbox starts charging without user input when a car is plugged in. We don't want that.
            // Make sure the wallbox is only charging when the power state is set to true.
            if (!thing->stateValue(webastoNextPowerStateTypeId).toBool()) {
                QModbusReply *reply = webastoNextConnection->setChargingAction(WebastoNextModbusTcpConnection::ChargingActionCancelSession);
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                connect(reply, &QModbusReply::finished, webastoNextConnection, [reply, webastoNextConnection](){
                    if (reply->error() == QModbusDevice::NoError) {
                        qCDebug(dcWebasto()) << "Wallbox" << webastoNextConnection->hostAddress() << "should not be charging. Sent stop charging command.";
                    } else {
                        qCWarning(dcWebasto()) << "Tried to send stop charging command to wallbox" << webastoNextConnection->hostAddress() << ", got error:" << reply->errorString();
                    }
                });
            }

            break;
        }

        // Set states only from one input. So we don't need this.
        /*
        switch (webastoNextConnection->chargerState()) {
        case WebastoNextModbusTcpConnection::ChargerStateNoVehicle:
            thing->setStateValue(webastoNextChargingStateTypeId, false);
            thing->setStateValue(webastoNextPluggedInStateTypeId, false);
            break;
        case WebastoNextModbusTcpConnection::ChargerStateVehicleAttachedNoPermission:
            thing->setStateValue(webastoNextPluggedInStateTypeId, true);
            break;
        case WebastoNextModbusTcpConnection::ChargerStateCharging:
            thing->setStateValue(webastoNextChargingStateTypeId, true);
            thing->setStateValue(webastoNextPluggedInStateTypeId, true);
            break;
        case WebastoNextModbusTcpConnection::ChargerStateChargingPaused:
            thing->setStateValue(webastoNextPluggedInStateTypeId, true);
            break;
        default:
            break;
        }
        */

        switch (webastoNextConnection->cableState()) {
        case WebastoNextModbusTcpConnection::CableStateNoCableAttached:
            thing->setStateValue(webastoNextPluggedInStateTypeId, false);
            break;
        case WebastoNextModbusTcpConnection::CableStateCableAttachedNoCar:
            thing->setStateValue(webastoNextPluggedInStateTypeId, false);
            break;
        case WebastoNextModbusTcpConnection::CableStateCableAttachedCarAttached:
            thing->setStateValue(webastoNextPluggedInStateTypeId, true);
            break;
        case WebastoNextModbusTcpConnection::CableStateCableAttachedCarAttachedLocked:
            thing->setStateValue(webastoNextPluggedInStateTypeId, true);
            break;
        default:
            break;
        }

        // Meter values
        thing->setStateValue(webastoNextCurrentPowerPhaseAStateTypeId, webastoNextConnection->activePowerL1());
        thing->setStateValue(webastoNextCurrentPowerPhaseBStateTypeId, webastoNextConnection->activePowerL2());
        thing->setStateValue(webastoNextCurrentPowerPhaseCStateTypeId, webastoNextConnection->activePowerL3());

        double currentPhaseA = webastoNextConnection->currentL1() / 1000.0;
        double currentPhaseB = webastoNextConnection->currentL2() / 1000.0;
        double currentPhaseC = webastoNextConnection->currentL3() / 1000.0;
        thing->setStateValue(webastoNextCurrentPhaseAStateTypeId, currentPhaseA);
        thing->setStateValue(webastoNextCurrentPhaseBStateTypeId, currentPhaseB);
        thing->setStateValue(webastoNextCurrentPhaseCStateTypeId, currentPhaseC);

        // Note: we do not use the active phase power, because we have sometimes a few watts on inactive phases.
        Electricity::Phases phases = Electricity::PhaseNone;
        phases.setFlag(Electricity::PhaseA, currentPhaseA > 1);
        phases.setFlag(Electricity::PhaseB, currentPhaseB > 1);
        phases.setFlag(Electricity::PhaseC, currentPhaseC > 1);
        if (phases != Electricity::PhaseNone) {
            thing->setStateValue(webastoNextUsedPhasesStateTypeId, Electricity::convertPhasesToString(phases));
            thing->setStateValue(webastoNextPhaseCountStateTypeId, Electricity::getPhaseCount(phases));
        } else {
            thing->setStateValue(webastoNextPhaseCountStateTypeId, 1);
        }


        thing->setStateValue(webastoNextCurrentPowerStateTypeId, webastoNextConnection->totalActivePower());

        thing->setStateValue(webastoNextTotalEnergyConsumedStateTypeId, webastoNextConnection->energyConsumed() / 1000.0);
        thing->setStateValue(webastoNextSessionEnergyStateTypeId, webastoNextConnection->sessionEnergy() / 1000.0);

        // Min / Max charging current
        thing->setStateValue(webastoNextMinCurrentTotalStateTypeId, webastoNextConnection->minChargingCurrent());
        thing->setStateValue(webastoNextMaxCurrentTotalStateTypeId, webastoNextConnection->maxChargingCurrent());

        thing->setStateMinValue(webastoNextMaxChargingCurrentStateTypeId, webastoNextConnection->minChargingCurrent());
        thing->setStateMaxValue(webastoNextMaxChargingCurrentStateTypeId, webastoNextConnection->maxChargingCurrent());

        thing->setStateValue(webastoNextMaxCurrentChargerStateTypeId, webastoNextConnection->maxChargingCurrentStation());
        thing->setStateValue(webastoNextMaxCurrentCableStateTypeId, webastoNextConnection->maxChargingCurrentCable());
        thing->setStateValue(webastoNextMaxCurrentElectricVehicleStateTypeId, webastoNextConnection->maxChargingCurrentEv());

        if (webastoNextConnection->evseErrorCode() == 0) {
            thing->setStateValue(webastoNextErrorStateTypeId, "");
        } else {
            uint errorCode = webastoNextConnection->evseErrorCode() - 1;
            switch (errorCode) {
            case 1:
                // Note: also PB61 has the same mapping and the same reason for the error.
                // We inform only about the PB02 since it does not make any difference regarding the action
                thing->setStateValue(webastoNextErrorStateTypeId, "PB02 - PowerSwitch Failure");
                break;
            case 2:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB07 - InternalError (Aux Voltage)");
                break;
            case 3:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB09 - EV Communication Error");
                break;
            case 4:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB17 - OverVoltage");
                break;
            case 5:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB18 - UnderVoltage");
                break;
            case 6:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB23 - OverCurrent Failure");
                break;
            case 7:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB24 - OtherError");
                break;
            case 8:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB27 - GroundFailure");
                break;
            case 9:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB28 - InternalError (Selftest)");
                break;
            case 10:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB29 - High Temperature");
                break;
            case 11:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB52 - Proximity Pilot Error");
                break;
            case 12:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB53 - Shutter Error");
                break;
            case 13:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB57 - Error Three Phase Check");
                break;
            case 14:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB59 - PWR internal error");
                break;
            case 15:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB60 - EV Communication Error - Negative control pilot voltage");
                break;
            case 16:
                thing->setStateValue(webastoNextErrorStateTypeId, "PB62- DC residual current (Vehicle)");
                break;
            default:
                thing->setStateValue(webastoNextErrorStateTypeId, QString("Unknwon error code %1").arg(errorCode));
                break;
            }
        }

        // Handle life bit (keep alive mechanism if there is a HEMS activated)
        if (webastoNextConnection->lifeBit() == 0) {
            // Let's reset the life bit so the wallbox knows we are still here,
            // otherwise the wallbox goes into the failsave mode and limits the charging to the configured
            QModbusReply *reply = webastoNextConnection->setLifeBit(1);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, webastoNextConnection, [reply, webastoNextConnection](){
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcWebasto()) << "Resetted life bit watchdog on" << webastoNextConnection->hostAddress() << "finished successfully";
                } else {
                    qCWarning(dcWebasto()) << "Resetted life bit watchdog on" << webastoNextConnection->hostAddress() << "finished with error:" << reply->errorString();
                }
            });
        }
    });


    connect(webastoNextConnection, &WebastoNextModbusTcpConnection::comTimeoutChanged, thing, [thing](quint16 comTimeout){
        thing->setStateValue(webastoNextCommunicationTimeoutStateTypeId, comTimeout);
    });

    connect(webastoNextConnection, &WebastoNextModbusTcpConnection::safeCurrentChanged, thing, [thing](quint16 safeCurrent){
        thing->setStateValue(webastoNextSafeCurrentStateTypeId, safeCurrent);
    });

    qCInfo(dcWebasto()) << "Setup finished successfully for Webasto NEXT" << thing << monitor;
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginWebasto::executeWebastoNextPowerAction(ThingActionInfo *info, bool power)
{
    qCDebug(dcWebasto()) << (power ? "Enabling": "Disabling") << "charging on" << info->thing();

    WebastoNextModbusTcpConnection *connection = m_webastoNextConnections.value(info->thing());
    QModbusReply *reply = nullptr;
    if (power) {
        reply = connection->setChargingAction(WebastoNextModbusTcpConnection::ChargingActionStartSession);
    } else {
        reply = connection->setChargingAction(WebastoNextModbusTcpConnection::ChargingActionCancelSession);
    }

    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, info, [info, reply, power](){
        if (reply->error() == QModbusDevice::NoError) {
            info->thing()->setStateValue(webastoNextPowerStateTypeId, power);
            qCDebug(dcWebasto()) << "Enabling/disabling charging request finished successfully.";
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCWarning(dcWebasto()) << "Enabling/disabling charging request finished with error:" << reply->errorString();
            info->finish(Thing::ThingErrorHardwareFailure);
        }
    });
}

void IntegrationPluginWebasto::setupEVC04Connection(ThingSetupInfo *info)
{
    Thing *thing = info->thing();

    QHostAddress address = m_monitors.value(thing)->networkDeviceInfo().address();

    qCDebug(dcWebasto()) << "Setting up EVC04 wallbox on" << address.toString();
    EVC04ModbusTcpConnection *evc04Connection = new EVC04ModbusTcpConnection(address, 502, 0x1f, this);
    connect(info, &ThingSetupInfo::aborted, evc04Connection, &EVC04ModbusTcpConnection::deleteLater);

    // Reconnect on monitor reachable changed
    NetworkDeviceMonitor *monitor = m_monitors.value(thing);
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool monitorReachable){

        if (monitorReachable) {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "is now reachable at" << monitor->networkDeviceInfo();
        } else {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_evc04Connections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue("connected").toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitorReachable) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcWebasto()) << "Connection is not working. Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_evc04Connections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_evc04Connections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcWebasto()) << "The device is offline. Stopping communication attempts.";
                    m_evc04Connections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(evc04Connection, &EVC04ModbusTcpConnection::reachableChanged, thing, [this, thing, evc04Connection, monitor](bool reachable){
        if (m_uniteDiscoveryRunning == true) {
            // Don't do anything if this triggers because of a discovery.
            return;
        }

        qCDebug(dcWebasto()) << "Reachable changed to" << reachable << "for" << thing << ". Monitor is" << monitor->reachable();

        if (reachable) {
            evc04Connection->initialize();
        } else {
            thing->setStateValue("connected", false);
            thing->setStateValue("currentPower", 0);

            // Check the monitor. If the monitor is reachable, get the current IP (maybe it changed) and reconnect.
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_evc04Connections.contains(thing)) {
                    qCDebug(dcWebasto()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_evc04Connections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_evc04Connections.value(thing)->reconnectDevice();
                }
            } else {
                // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                m_evc04Connections.value(thing)->disconnectDevice();
            }
        }
    });

    // This is needed because the monitor is sometimes too slow to trigger reachableChanged when the address changed.
    connect(monitor, &NetworkDeviceMonitor::networkDeviceInfoChanged, thing, [=](NetworkDeviceInfo networkDeviceInfo){

        if (monitor->reachable()) {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "has changed it's network address and is now reachable at" << networkDeviceInfo;
        } else {
            qCDebug(dcWebasto()) << "Monitor detected a change -" << thing << "has changed it's network address and is offline";
        }

        if (!thing->setupComplete())
            return;

        // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
        if (m_evc04Connections.contains(thing)) {
            // The monitor is not very reliable. Sometimes it says monitor is not reachable, even when the connection ist still working.
            // So we need to test if the connection is actually not working before triggering a reconnect. Don't reconnect when the connection is actually working.
            if (!thing->stateValue("connected").toBool()) {
                // connectedState switches to false when modbus calls don't work (webastoNextConnection->reachable == false).
                if (monitor->reachable()) {
                    // Modbus communication is not working. Monitor says device is reachable. Set IP again (maybe it changed), then reconnect.
                    qCDebug(dcWebasto()) << "Connection is not working. Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_evc04Connections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_evc04Connections.value(thing)->reconnectDevice();
                } else {
                    // Modbus is not working and the monitor is not reachable. We can stop sending modbus calls now.
                    qCDebug(dcWebasto()) << "The device is offline. Stopping communication attempts.";
                    m_evc04Connections.value(thing)->disconnectDevice();
                }
            }
        }
    });

    connect(evc04Connection, &EVC04ModbusTcpConnection::initializationFinished, thing, [this, thing, evc04Connection, monitor](bool success){
        if (!thing->setupComplete())
            return;

        if (success) {
            thing->setStateValue("connected", true);
        } else {
            qCDebug(dcWebasto()) << "Initialization failed";
            thing->setStateValue("connected", false);
            thing->setStateValue("currentPower", 0);

            // Try once to reconnect the device
            if (monitor->reachable()) {
                // Get the connection from the list. If thing is not in the list, something else is going on and we should not reconnect.
                if (m_evc04Connections.contains(thing)) {
                    qCDebug(dcWebasto()) << "Setting thing IP address to" << monitor->networkDeviceInfo().address() << "and reconnecting.";
                    m_evc04Connections.value(thing)->setHostAddress(monitor->networkDeviceInfo().address());
                    m_evc04Connections.value(thing)->reconnectDevice();
                }
            }
        }
    });

    connect(evc04Connection, &EVC04ModbusTcpConnection::initializationFinished, info, [=](bool success){
        if (!success) {
            qCWarning(dcWebasto()) << "Connection init finished with errors" << thing->name() << evc04Connection->hostAddress().toString();
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
            evc04Connection->deleteLater();
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error communicating with the wallbox."));
            return;
        }

        qCDebug(dcWebasto()) << "Connection init finished successfully" << evc04Connection;

        m_evc04Connections.insert(thing, evc04Connection);
        info->finish(Thing::ThingErrorNoError);

        thing->setStateValue("connected", true);
        thing->setStateValue("version", QString(QString::fromUtf16(evc04Connection->firmwareVersion().data(), evc04Connection->firmwareVersion().length()).toUtf8()).trimmed());

        m_timeoutCount[thing] = 0;
        evc04Connection->update();
    });

    connect(evc04Connection, &EVC04ModbusTcpConnection::updateFinished, thing, [this, evc04Connection, thing](){
        if (!evc04Connection->connected()) {
            qCDebug(dcWebasto()) << "Skipping EVC04 updateFinished, device is not connected.";
            return;
        }

        //qCDebug(dcWebasto()) << "EVC04 update finished:" << thing->name() << evc04Connection;
        qCDebug(dcWebasto()) << "EVC04 update finished:" << thing->name();

        //qCDebug(dcWebasto()) << "Serial:" << QString(QString::fromUtf16(evc04Connection->serialNumber().data(), evc04Connection->serialNumber().length()).toUtf8()).trimmed();
        //qCDebug(dcWebasto()) << "ChargePoint ID:" << QString(QString::fromUtf16(evc04Connection->chargepointId().data(), evc04Connection->chargepointId().length()).toUtf8()).trimmed();
        //qCDebug(dcWebasto()) << "Brand:" << QString(QString::fromUtf16(evc04Connection->brand().data(), evc04Connection->brand().length()).toUtf8()).trimmed();
        //qCDebug(dcWebasto()) << "Model:" << QString(QString::fromUtf16(evc04Connection->model().data(), evc04Connection->model().length()).toUtf8()).trimmed();

        updateEVC04MaxCurrent(thing, evc04Connection);

        double currentPhaseA = evc04Connection->currentL1() / 1000.0;
        double currentPhaseB = evc04Connection->currentL2() / 1000.0;
        double currentPhaseC = evc04Connection->currentL3() / 1000.0;
        thing->setStateValue("currentPhaseA", currentPhaseA);
        thing->setStateValue("currentPhaseB", currentPhaseB);
        thing->setStateValue("currentPhaseC", currentPhaseC);

        quint16 phaseCount{0};
        if (currentPhaseA > 1) {
            phaseCount++;
        }
        if (currentPhaseB > 1) {
            phaseCount++;
        }
        if (currentPhaseC > 1) {
            phaseCount++;
        }

        // phaseCount not allowed to be 0 in current version of ConEMS
        if (phaseCount < 1) {
            phaseCount = 1;
        }
        thing->setStateValue("phaseCount", phaseCount);

        quint32 evseFaultCode = evc04Connection->evseFaultCode();
        if (evseFaultCode == 0) {
            thing->setStateValue("evseFaultCode", "No error");
        } else {
            thing->setStateValue("evseFaultCode", evseFaultCode);
        }

        // The wallbox starts charging as soon as you plug in the car. We don't want that. We want it to only charge when Nymea says so.
        quint16 chargingCurrent = evc04Connection->chargingCurrent();
        bool power = thing->stateValue("power").toBool();
        if (chargingCurrent > 1 && !power) {
            QModbusReply *reply = evc04Connection->setChargingCurrent(0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        }

        // I've been observing the wallbox getting stuck on modbus. It is still functional, but modbus keeps on returning the same old values
        // until the TCP connection is closed and reopened. Checking the wallbox time register to detect that and auto-reconnect.
        if (m_lastWallboxTime[thing] == evc04Connection->time()) {
            quint16 count = m_timeoutCount[thing];
            count++;
            m_timeoutCount[thing] = count;
            qCDebug(dcWebasto()) << "Time value did not update, count" << count;
            if (count >= 3) {
                qCWarning(dcWebasto()) << "Time value did not update three times. Wallbox seems stuck and returning outdated values. Reconnecting...";
                evc04Connection->reconnectDevice();
            }
        } else {
            m_timeoutCount[thing] = 0;
        }
        m_lastWallboxTime[thing] = evc04Connection->time();
    });

    connect(evc04Connection, &EVC04ModbusTcpConnection::chargepointStateChanged, thing, [thing](EVC04ModbusTcpConnection::ChargePointState chargePointState) {
        qCDebug(dcWebasto()) << "Chargepoint state changed" << chargePointState;
//        switch (chargePointState) {
//        case EVC04ModbusTcpConnection::ChargePointStateAvailable:
//        case EVC04ModbusTcpConnection::ChargePointStatePreparing:
//        case EVC04ModbusTcpConnection::ChargePointStateReserved:
//        case EVC04ModbusTcpConnection::ChargePointStateUnavailable:
//        case EVC04ModbusTcpConnection::ChargePointStateFaulted:
//            thing->setStateValue(evc04PluggedInStateTypeId, false);
//            break;
//        case EVC04ModbusTcpConnection::ChargePointStateCharging:
//        case EVC04ModbusTcpConnection::ChargePointStateSuspendedEVSE:
//        case EVC04ModbusTcpConnection::ChargePointStateSuspendedEV:
//        case EVC04ModbusTcpConnection::ChargePointStateFinishing:
//            thing->setStateValue(evc04PluggedInStateTypeId, true);
//            break;
//        }
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::chargingStateChanged, thing, [thing](EVC04ModbusTcpConnection::ChargingState chargingState) {
        qCDebug(dcWebasto()) << "Charging state changed:" << chargingState;
        thing->setStateValue("charging", chargingState == EVC04ModbusTcpConnection::ChargingStateCharging);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::activePowerTotalChanged, thing, [thing](quint16 activePowerTotal) {
        qCDebug(dcWebasto()) << "Total active power:" << activePowerTotal;
        // The wallbox reports some 5-6W even when there's nothing connected. Let's hide that if we're not charging
        if (thing->stateValue("charging").toBool() == true) {
            thing->setStateValue("currentPower", activePowerTotal);
        } else {
            thing->setStateValue("currentPower", 0);
        }
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::meterReadingChanged, thing, [thing](quint32 meterReading) {
        qCDebug(dcWebasto()) << "Meter reading changed:" << meterReading;
        thing->setStateValue("totalEnergyConsumed", meterReading / 10.0);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::sessionMaxCurrentChanged, thing, [](quint16 sessionMaxCurrent) {
        // This mostly just reflects what we've been writing to cargingCurrent, so not of much use...
        qCDebug(dcWebasto()) << "Session max current changed:" << sessionMaxCurrent;
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::cableMaxCurrentChanged, thing, [this, evc04Connection, thing](quint16 cableMaxCurrent) {
        qCDebug(dcWebasto()) << "Cable max current changed:" << cableMaxCurrent;
        updateEVC04MaxCurrent(thing, evc04Connection);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::evseMinCurrentChanged, thing, [thing](quint16 evseMinCurrent) {
        qCDebug(dcWebasto()) << "EVSE min current changed:" << evseMinCurrent;
        thing->setStateMinValue("maxChargingCurrent", evseMinCurrent);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::evseMaxCurrentChanged, thing, [this, evc04Connection, thing](quint16 evseMaxCurrent) {
        qCDebug(dcWebasto()) << "EVSE max current changed:" << evseMaxCurrent;
        updateEVC04MaxCurrent(thing, evc04Connection);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::sessionEnergyChanged, thing, [thing](quint32 sessionEnergy) {
        qCDebug(dcWebasto()) << "Session energy changed:" << sessionEnergy;
        thing->setStateValue("sessionEnergy", sessionEnergy / 1000.0);
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::chargingCurrentChanged, thing, [thing](quint16 chargingCurrent) {
        qCDebug(dcWebasto()) << "Charging current changed:" << chargingCurrent;

        // This wallbox is turned off by setting the charging current to 0. Only set the value when it is not zero, otherwise turning the wallbox off will change this value.
        if (chargingCurrent > 1) {
            thing->setStateValue("maxChargingCurrent", chargingCurrent);
        }
    });
    connect(evc04Connection, &EVC04ModbusTcpConnection::cableStateChanged, thing, [evc04Connection, thing](EVC04ModbusTcpConnection::CableState cableState) {
        switch (cableState) {
        case EVC04ModbusTcpConnection::CableStateNotConnected:
        case EVC04ModbusTcpConnection::CableStateCableConnectedVehicleNotConnected:
            thing->setStateValue("pluggedIn", false);
            break;
        case EVC04ModbusTcpConnection::CableStateCableConnectedVehicleConnected:
        case EVC04ModbusTcpConnection::CableStateCableConnectedVehicleConnectedCableLocked:
            thing->setStateValue("pluggedIn", true);
            break;
        }
    });

    evc04Connection->connectDevice();
}

void IntegrationPluginWebasto::updateEVC04MaxCurrent(Thing *thing, EVC04ModbusTcpConnection *connection)
{
    quint16 wallboxMax = connection->maxChargePointPower() > 0 ? connection->maxChargePointPower() / 230 : 32;
    quint16 evseMax = connection->evseMaxCurrent() > 0 ? connection->evseMaxCurrent() : wallboxMax;
    quint16 cableMax = connection->cableMaxCurrent() > 0 ? connection->cableMaxCurrent() : wallboxMax;


    quint8 overallMax = qMin(qMin(wallboxMax, evseMax), cableMax);
    qCDebug(dcWebasto()) << "Adjusting max current: Wallbox max:" << wallboxMax << "EVSE max:" << evseMax << "cable max:" << cableMax << "Overall:" << overallMax;
    thing->setStateMinMaxValues("maxChargingCurrent", 6, overallMax);
}
