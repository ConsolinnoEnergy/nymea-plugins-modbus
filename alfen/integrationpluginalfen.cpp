/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
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

#include "integrationpluginalfen.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <types/param.h>
#include <plugintimer.h>

#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QNetworkInterface>


IntegrationPluginAlfen::IntegrationPluginAlfen()
{

}

void IntegrationPluginAlfen::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == alfenEveSingleProThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcAlfen()) << "Failed to discover network devices. The network device discovery is not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available. Please enter the IP address manually."));
            return;
        }

        qCDebug(dcAlfen()) << "Starting network discovery...";
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, info, [=](){
            qCDebug(dcAlfen()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
                qCDebug(dcAlfen()) << networkDeviceInfo;

                QString title;
                if (networkDeviceInfo.hostName().isEmpty()) {
                    title = networkDeviceInfo.address().toString();
                } else {
                    title = networkDeviceInfo.hostName() + " (" + networkDeviceInfo.address().toString() + ")";
                }

                QString description;
                if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                    description = networkDeviceInfo.macAddress();
                } else {
                    description = networkDeviceInfo.macAddress() + " (" + networkDeviceInfo.macAddressManufacturer() + ")";
                }

                ThingDescriptor descriptor(info->thingClassId(), title, description);
                ParamList params;
                params << Param(alfenEveSingleProThingMacParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(alfenEveSingleProThingMacParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcAlfen()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });
    }
}


void IntegrationPluginAlfen::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcAlfen()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == alfenEveSingleProThingClassId) {
        Thing *thing = info->thing();

        MacAddress mac = MacAddress(thing->paramValue(alfenEveSingleProThingMacParamTypeId).toString());
        if (mac.isNull()) {
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The given MAC address is not valid."));
            return;
        }
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(mac);

        uint port = thing->paramValue(alfenEveSingleProThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(alfenEveSingleProThingSlaveIdParamTypeId).toUInt();
        AlfenWallboxModbusTcpConnection *alfenWallboxTcpConnection = new AlfenWallboxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        //alfenWallboxTcpConnection = new AlfenWallboxModbusTcpConnection(monitor->networkDeviceInfo().address(), port, slaveId, this);
        //SchneiderWallbox *schneiderWallbox = new SchneiderWallbox(alfenWallboxTcpConnection, this);
        //connect(info, &ThingSetupInfo::aborted, schneiderWallbox, &SchneiderWallbox::deleteLater);
        connect(info, &ThingSetupInfo::aborted, monitor, [monitor, this](){ hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);});

        quint16 minCurrentLimit = thing->paramValue(alfenEveSingleProThingMinChargeCurrentParamTypeId).toUInt();
        quint16 maxCurrentLimit = thing->paramValue(alfenEveSingleProThingMaxChargeCurrentParamTypeId).toUInt();
        thing->setStateMinMaxValues(alfenEveSingleProMaxChargingCurrentStateTypeId, minCurrentLimit, maxCurrentLimit);

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::reachableChanged, thing, [alfenWallboxTcpConnection, thing](bool reachable){
            qCDebug(dcAlfen()) << "Reachable state changed" << reachable;
            if (reachable) {
                alfenWallboxTcpConnection->initialize();
            } else {
                thing->setStateValue(alfenEveSingleProConnectedStateTypeId, false);
            }
        });

        // Only during setup
        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::initializationFinished, info, [this, thing, monitor, alfenWallboxTcpConnection, info](bool success){
            if (success) {
                qCDebug(dcAlfen()) << "Alfen wallbox initialized.";
                //m_schneiderDevices.insert(thing, schneiderWallbox);
                m_modbusTcpConnections.insert(thing, alfenWallboxTcpConnection);
                m_monitors.insert(thing, monitor);
                info->finish(Thing::ThingErrorNoError);
            } else {
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(monitor);
                //schneiderWallbox->deleteLater();
                info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Could not initialize the communication with the wallbox."));
            }
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::updateFinished, thing, [alfenWallboxTcpConnection, thing](){
            qCDebug(dcAlfen()) << "Update finished:" << thing->name() << alfenWallboxTcpConnection;
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::initializationFinished, thing, [thing](bool success){
            if (success) {
                thing->setStateValue(alfenEveSingleProConnectedStateTypeId, true);
            }
        });

        // Handle property changed signals

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::mode3StateChanged, this, [thing, this](QString state){
            qCDebug(dcAlfen()) << thing << "Charge point state changed" << state;
            thing->setStateValue(alfenEveSingleProMode3StateStateTypeId, chargePointStates[state]);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::availabilityChanged, this, [thing, this](AlfenWallboxModbusTcpConnection::Availability availability){
            qCDebug(dcAlfen()) << thing << "Availability changed" << availability;
            switch (availability) {
                case AlfenWallboxModbusTcpConnection::AvailabilityOperative:
                    thing->setStateValue(alfenEveSingleProAvailabilityStateTypeId, "Operative");
                    thing->setStateValue(alfenEveSingleProConnectedStateTypeId, true);
                    break;
                case AlfenWallboxModbusTcpConnection::AvailabilityInoperative:
                    thing->setStateValue(alfenEveSingleProAvailabilityStateTypeId, "Inoperative");
                    thing->setStateValue(alfenEveSingleProConnectedStateTypeId, false);
                    break;
            }
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::realPowerSumChanged, this, [thing](float power){
            qCDebug(dcAlfen()) << thing << "power changed" << power << "W";
            thing->setStateValue(alfenEveSingleProCurrentPowerStateTypeId, power);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::currentSumChanged, this, [thing](float current){
            qCDebug(dcAlfen()) << thing << "current changed" << current << "A";
            thing->setStateValue(alfenEveSingleProCurrentStateTypeId, current);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::realEnergyConsumedSumChanged, this, [thing](float energy){
            qCDebug(dcAlfen()) << thing << "total energy consumed changed" << energy << "Wh";
            thing->setStateValue(alfenEveSingleProTotalEnergyConsumedStateTypeId, energy);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::realEnergyDeliveredSumChanged, this, [thing](float energy){
            qCDebug(dcAlfen()) << thing << "total energy delivered changed" << energy << "Wh";
            thing->setStateValue(alfenEveSingleProTotalEnergyDeliveredStateTypeId, energy);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::phaseUsedChanged, this, [thing, this](quint16 phaseCount){
            qCDebug(dcAlfen()) << thing << "Phase count changed" << phaseCount ;
            thing->setStateValue(alfenEveSingleProPhaseCountStateTypeId, phaseCount);
        });

        connect(alfenWallboxTcpConnection, &AlfenWallboxModbusTcpConnection::actualAppliedMaxCurrentChanged, this, [thing, this](float maxCurrent){
            qCDebug(dcAlfen()) << thing << "Max current changed" << maxCurrent ;
            thing->setStateValue(alfenEveSingleProMaxChargingCurrentStateTypeId, (int)maxCurrent);
            if (maxCurrent < 6) {
                thing->setStateValue(alfenEveSingleProPowerStateTypeId, false);
            } else {
                thing->setStateValue(alfenEveSingleProPowerStateTypeId, true);
            }
        });

        alfenWallboxTcpConnection->connectDevice();

    }
}


void IntegrationPluginAlfen::postSetupThing(Thing *thing)
{
    qCDebug(dcAlfen()) << "Post setup" << thing->name();
    if (thing->thingClassId() != alfenEveSingleProThingClassId) {
        qCWarning(dcAlfen()) << "Thing class id not supported " << thing->thingClassId();
        return;
    }

    if (!m_pluginTimer) {
        qCDebug(dcAlfen()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(Thing *thing, myThings()) {
                if (m_monitors.value(thing)->reachable()) {
                    qCDebug(dcAlfen()) << "Updating" << thing->name();
                    //m_schneiderDevices.value(thing)->modbusTcpConnection()->update();
                    //m_schneiderDevices.value(thing)->update();
                } else {
                    qCDebug(dcAlfen()) << thing->name() << "isn't reachable. Not updating.";
                }
            }
        });

    }
}

void IntegrationPluginAlfen::thingRemoved(Thing *thing)
{
    qCDebug(dcAlfen()) << "Removing device" << thing->name();

    if (myThings().isEmpty() && m_pluginTimer) {
        qCDebug(dcAlfen()) << "Stopping plugin timers ...";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
    /*if (myThings().isEmpty() && m_monitors) {
    qCDebug(dcAlfen()) << "Stopping plugin monitors ...";
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        m_monitors = nullptr;
    }*/
}

void IntegrationPluginAlfen::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();


    if (thing->thingClassId() == alfenEveSingleProThingClassId) {

        AlfenWallboxModbusTcpConnection *alfenWallboxTcpConnection = m_modbusTcpConnections.value(thing);
        if (!alfenWallboxTcpConnection) {
            qCWarning(dcAlfen()) << "Modbus connection not available";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        if (!alfenWallboxTcpConnection->connected()) {
            qCWarning(dcAlfen()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }


        //SchneiderWallbox *device = m_schneiderDevices.value(thing);
        bool success = false;
        if (action.actionTypeId() == alfenEveSingleProPowerActionTypeId) {
            bool state = action.paramValue(alfenEveSingleProPowerActionPowerParamTypeId).toBool();
            if (state) {
                // only once on startup when ampereBefore is not yet set
                if (!ampereValueBefore) {
                    ampereValueBefore = thing->stateValue(alfenEveSingleProMaxChargingCurrentStateTypeId).toUInt();
                }
                // send before value again to allow charging again
                success = setMaxCurrent(alfenWallboxTcpConnection, ampereValueBefore);
            } else {
                // get before value
                ampereValueBefore = thing->stateValue(alfenEveSingleProMaxChargingCurrentStateTypeId).toUInt();
                // send 0 ampere to stop charging
                success = setMaxCurrent(alfenWallboxTcpConnection, 0);
            }

        } else if (action.actionTypeId() == alfenEveSingleProMaxChargingCurrentActionTypeId) {
            int ampereValue = action.paramValue(alfenEveSingleProMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            success = setMaxCurrent(alfenWallboxTcpConnection, ampereValue);
            thing->setStateValue(alfenEveSingleProMaxChargingCurrentStateTypeId, ampereValue);

        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
        }

        if (success) {
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCWarning(dcAlfen()) << "Action execution finished with error.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }
    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }
}

bool IntegrationPluginAlfen::setMaxCurrent(AlfenWallboxModbusTcpConnection *connection, int maxCurrent) {
    QModbusReply *reply = connection->setSetpointMaxCurrent((float)maxCurrent);
    if (!reply) {
        qCWarning(dcAlfen()) << "Sending max current failed because the reply could not be created.";
        //m_errorOccured = true;
        return false;
    }
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::errorOccurred, this, [reply] (QModbusDevice::Error error){
        qCWarning(dcAlfen()) << "Modbus reply error occurred while sending max current" << error << reply->errorString();
        emit reply->finished(); // To make sure it will be deleted
    });
    return true;
}