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

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

#include "integrationpluginmennekes.h"
#include "plugininfo.h"

IntegrationPluginMennekes::IntegrationPluginMennekes()
{

}

void IntegrationPluginMennekes::init()
{

}

void IntegrationPluginMennekes::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcMennekesWallbox()) << "The network discovery does not seem to be available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available. Please enter the IP address manually."));
        return;
    }

    if (info->thingClassId() == mennekesWallboxThingClassId) {

        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

                qCDebug(dcMennekesWallbox()) << "Found" << networkDeviceInfo;

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

                ThingDescriptor descriptor(mennekesWallboxThingClassId, title, description);
                ParamList params;
                params << Param(mennekesWallboxThingIpParamTypeId, networkDeviceInfo.address().toString());
                params << Param(mennekesWallboxThingMacParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(mennekesWallboxThingIpParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcMennekesWallbox()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });
    } else {
        qCWarning(dcMennekesWallbox()) << "Could not discover things because of unhandled thing class id" << info->thingClassId().toString();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}


void IntegrationPluginMennekes::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == mennekesWallboxThingClassId) {

        // Handle reconfigure
        if (myThings().contains(thing)) {
            MennekesWallbox *mennekesWallbox = m_wallboxDevices.take(thing->id());
            if (mennekesWallbox) {
                qCDebug(dcMennekesWallbox()) << "Reconfigure" << thing->name() << thing->params();
                delete mennekesWallbox;
                // Now continue with the normal setup
            }
        }

        qCDebug(dcMennekesWallbox()) << "Setup" << thing << thing->params();

        QHostAddress hostAddress = QHostAddress(thing->paramValue(mennekesWallboxThingIpParamTypeId).toString());
        if (hostAddress.isNull()){
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given"));
            return;
        }

        uint port = thing->paramValue(mennekesWallboxThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(mennekesWallboxThingSlaveIdParamTypeId).toUInt();

        quint16 minCurrentLimit = thing->paramValue(mennekesWallboxThingMinChargeCurrentParamTypeId).toUInt();
        quint16 maxCurrentLimit = thing->paramValue(mennekesWallboxThingMaxChargeCurrentParamTypeId).toUInt();
        thing->setStateMinMaxValues(mennekesWallboxMaxChargingCurrentStateTypeId, minCurrentLimit, maxCurrentLimit);

        // Check if we have a MennekesWallbox with this ip and slaveId, if reconfigure the object would already been removed from the hash
        foreach (MennekesWallbox *mennekesWallboxEntry, m_wallboxDevices.values()) {
            if (mennekesWallboxEntry->modbusTcpConnection()->hostAddress() == hostAddress && mennekesWallboxEntry->slaveId() == slaveId) {
                qCWarning(dcMennekesWallbox()) << "Failed to set up MennekesWallbox for host address" << hostAddress.toString() << "and slaveId" << slaveId << "because there has already been configured a MennekesWallbox for this IP and slaveId.";
                info->finish(Thing::ThingErrorThingInUse, QT_TR_NOOP("Already configured for this IP address and slaveId."));
                return;
            }
        }

        // ToDO: Check if this is really a Schneider wallbox.

        MennekesModbusTcpConnection *mennekesConnectTcpConnection = new MennekesModbusTcpConnection(hostAddress, port, slaveId, this);
        connect(mennekesConnectTcpConnection, &MennekesModbusTcpConnection::connectionStateChanged, this, [thing, mennekesConnectTcpConnection](bool status){
            qCDebug(dcMennekesWallbox()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                mennekesConnectTcpConnection->update();
            }
            thing->setStateValue(mennekesWallboxConnectedStateTypeId, status);
        });

        connect(mennekesConnectTcpConnection, &MennekesModbusTcpConnection::chargeDurationChanged, this, [thing](quint16 chargeDuration){
            quint16 chargeTimeMinutes{static_cast<quint16>(chargeDuration / 60)};
            qCDebug(dcMennekesWallbox()) << thing << "Charge duration changed" << chargeTimeMinutes << " minutes";
            thing->setStateValue(mennekesWallboxChargeTimeStateTypeId, chargeTimeMinutes);
        });
        connect(mennekesConnectTcpConnection, &MennekesModbusTcpConnection::ocppStatusChanged, this, [thing, this](MennekesModbusTcpConnection::OCPPstatus ocppStatus){
            qCDebug(dcMennekesWallbox()) << thing << "OCPP state changed" << ocppStatus;
            setOcppState(thing, ocppStatus);
        });
        connect(mennekesConnectTcpConnection, &MennekesModbusTcpConnection::errorCode1Changed, this, [thing, this](quint32 errorCode1){
            qCDebug(dcMennekesWallbox()) << thing << "Error code 1 changed" << errorCode1;
            setErrorMessage(thing, errorCode1);
        });
        connect(mennekesConnectTcpConnection, &MennekesModbusTcpConnection::chargedEnergyChanged, this, [thing](quint16 chargedEnergyWh){
            double chargedEnergykWh{static_cast<double>(chargedEnergyWh / 1000.0)};
            qCDebug(dcMennekesWallbox()) << thing << "Session energy changed" << chargedEnergykWh << " kWh";
            thing->setStateValue(mennekesWallboxSessionEnergyStateTypeId, chargedEnergykWh);
        });

        mennekesConnectTcpConnection->connectDevice();

        MennekesWallbox *mennekesWallbox = new MennekesWallbox(mennekesConnectTcpConnection, slaveId, this);

        connect(mennekesWallbox, &MennekesWallbox::phaseCountChanged, this, [thing, this](quint16 phaseCount){
            qCDebug(dcMennekesWallbox()) << thing << "Phase count changed" << phaseCount ;
            setPhaseCount(thing, phaseCount);
        });
        connect(mennekesWallbox, &MennekesWallbox::currentPowerChanged, this, [thing, this](double currentPower){
            qCDebug(dcMennekesWallbox()) << thing << "Current power changed" << currentPower ;
            setCurrentPower(thing, currentPower);
        });

        m_wallboxDevices.insert(thing->id(), mennekesWallbox);

        connect(info, &ThingSetupInfo::aborted, mennekesWallbox, &MennekesWallbox::deleteLater); // Clean up if the setup fails
        connect(mennekesWallbox, &MennekesWallbox::destroyed, this, [thing, this]{
            m_wallboxDevices.remove(thing->id());
            //Todo: Setup failed, lets search the network, maybe the IP has changed...
        });


        info->finish(Thing::ThingErrorNoError);
    } else {
        qCWarning(dcMennekesWallbox()) << "Could not setup thing: unhandled device class" << thing->thingClass();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}


void IntegrationPluginMennekes::postSetupThing(Thing *thing)
{
    qCDebug(dcMennekesWallbox()) << "Post setup" << thing->name();
    if (thing->thingClassId() != mennekesWallboxThingClassId) {
        qCWarning(dcMennekesWallbox()) << "Thing class id not supported" << thing->thingClassId();
        return;
    }

    if (!m_pluginTimer) {
        qCDebug(dcMennekesWallbox()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(MennekesWallbox *device, m_wallboxDevices) {
                if (device->modbusTcpConnection()->connected()) {
                    device->modbusTcpConnection()->update();
                    device->update();
                }
            }
        });

        m_pluginTimer->start();
    }
}

void IntegrationPluginMennekes::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == mennekesWallboxThingClassId && m_wallboxDevices.contains(thing->id())) {
        qCDebug(dcMennekesWallbox()) << "Deleting" << thing->name();
        MennekesWallbox *device = m_wallboxDevices.take(thing->id());
        device->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        qCDebug(dcMennekesWallbox()) << "Stopping plugin timers ...";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginMennekes::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();


    if (thing->thingClassId() == mennekesWallboxThingClassId) {
        MennekesWallbox *device = m_wallboxDevices.value(thing->id());

        if (!device->modbusTcpConnection()->connected()) {
            qCWarning(dcMennekesWallbox()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        bool success = false;
        if (action.actionTypeId() == mennekesWallboxPowerActionTypeId) {
            bool onOff = action.paramValue(mennekesWallboxPowerActionPowerParamTypeId).toBool();
            success = device->enableOutput(onOff);
            thing->setStateValue(mennekesWallboxPowerStateTypeId, onOff);
            if (onOff) {
                // You can turn the wallbox on without specifying a charge current. The Nymea app saves the last current setpoint and displays it.
                // Need to get that saved value from the app and give it to mennekeswallbox.cpp so the displayed value matches the actual setpoint.
                int ampereValue = thing->stateValue(mennekesWallboxMaxChargingCurrentStateTypeId).toUInt();
                success = device->setMaxAmpere(ampereValue);
            }
        } else if(action.actionTypeId() == mennekesWallboxMaxChargingCurrentActionTypeId) {
            int ampereValue = action.paramValue(mennekesWallboxMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            success = device->setMaxAmpere(ampereValue);
            thing->setStateValue(mennekesWallboxMaxChargingCurrentStateTypeId, ampereValue);
        } else {
            qCWarning(dcMennekesWallbox()) << "Unhandled ActionTypeId:" << action.actionTypeId();
            return info->finish(Thing::ThingErrorActionTypeNotFound);
        }

        if (success) {
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCWarning(dcMennekesWallbox()) << "Action execution finished with error.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }
    } else {
        qCWarning(dcMennekesWallbox()) << "Execute action, unhandled device class" << thing->thingClass();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}

void IntegrationPluginMennekes::setOcppState(Thing *thing, MennekesModbusTcpConnection::OCPPstatus ocppStatus)
{
    bool isPluggedIn{false};    // ToDo: Wallbox states überprüfen, welche tatsächlich bei "Stecker steckt" angezeigt werden.
    switch (ocppStatus) {
    case MennekesModbusTcpConnection::OCPPstatusAvailable:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Available");
        break;
    case MennekesModbusTcpConnection::OCPPstatusOccupied:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Occupied");
        isPluggedIn = true;
        break;
    case MennekesModbusTcpConnection::OCPPstatusReserved:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Reserved");
        break;
    case MennekesModbusTcpConnection::OCPPstatusUnavailable:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Unavailable");
        break;
    case MennekesModbusTcpConnection::OCPPstatusFaulted:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Faulted");
        break;
    case MennekesModbusTcpConnection::OCPPstatusPreparing:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Preparing");
        break;
    case MennekesModbusTcpConnection::OCPPstatusCharging:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Charging");
        isPluggedIn = true;
        break;
    case MennekesModbusTcpConnection::OCPPstatusSuspendedEVSE:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Suspended EVSE");
        isPluggedIn = true;
        break;
    case MennekesModbusTcpConnection::OCPPstatusSuspendedEV:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Suspended EV");
        break;
    case MennekesModbusTcpConnection::OCPPstatusFinishing:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Finishing");
        break;
    default:
        thing->setStateValue(mennekesWallboxOcppStatusStateTypeId, "Unknown");
    }
    thing->setStateValue(mennekesWallboxPluggedInStateTypeId, isPluggedIn);
}

void IntegrationPluginMennekes::setCurrentPower(Thing *thing, double currentPower) {
    thing->setStateValue(mennekesWallboxCurrentPowerStateTypeId, currentPower);
    if (currentPower > 0.01) {
        thing->setStateValue(mennekesWallboxChargingStateTypeId, true);
    } else {
        thing->setStateValue(mennekesWallboxChargingStateTypeId, false);
    }
}

void IntegrationPluginMennekes::setPhaseCount(Thing *thing, quint16 phaseCount) {
    thing->setStateValue(mennekesWallboxPhaseCountStateTypeId, phaseCount);
    //qCDebug(dcMennekesWallbox()) << "Phase count set:" << phaseCount << " for thing:" << thing;
}

void IntegrationPluginMennekes::setErrorMessage(Thing *thing, quint32 errorBits) {
    if (!errorBits) {
        thing->setStateValue(mennekesWallboxErrorMessageStateTypeId, "No error");
    } else {
        QString errorMessage{""};
        quint32 testBit{0b01};
        if (errorBits & testBit) {
            errorMessage.append("Residual current detected via sensor, ");
        }
        if (errorBits & (testBit << 1)) {
            errorMessage.append("Vehicle signals error, ");
        }
        if (errorBits & (testBit << 2)) {
            errorMessage.append("Vehicle diode check failed - tamper detection, ");
        }
        if (errorBits & (testBit << 3)) {
            errorMessage.append("MCB of type 2 socket triggered, ");
        }
        if (errorBits & (testBit << 4)) {
            errorMessage.append("MCB of domestic socket triggered, ");
        }
        if (errorBits & (testBit << 5)) {
            errorMessage.append("RCD triggered, ");
        }
        if (errorBits & (testBit << 6)) {
            errorMessage.append("Contactor welded, ");
        }
        if (errorBits & (testBit << 7)) {
            errorMessage.append("Backend disconnected, ");
        }
        if (errorBits & (testBit << 8)) {
            errorMessage.append("Plug locking failed, ");
        }
        if (errorBits & (testBit << 9)) {
            errorMessage.append("Locking without plug error, ");
        }
        if (errorBits & (testBit << 10)) {
            errorMessage.append("Actuator stuck cannot unlock, ");
        }
        if (errorBits & (testBit << 11)) {
            errorMessage.append("Actuator detection failed, ");
        }
        if (errorBits & (testBit << 12)) {
            errorMessage.append("FW Update in progress, ");
        }
        if (errorBits & (testBit << 13)) {
            errorMessage.append("The charge point is tilted, ");
        }
        if (errorBits & (testBit << 14)) {
            errorMessage.append("CP/PR wiring issue, ");
        }
        if (errorBits & (testBit << 15)) {
            errorMessage.append("Car current overload, charging stopped, ");
        }
        if (errorBits & (testBit << 16)) {
            errorMessage.append("Actuator unlocked while charging, ");
        }

        if (errorBits & (testBit << 17)) {
            errorMessage.append("The charge point was tilted and it is not allowed to charge until the charge point is rebooted, ");
        }

        if (errorBits & (testBit << 18)) {
            errorMessage.append("PIC24 error, ");
        }

        if (errorBits & (testBit << 19)) {
            errorMessage.append("USB stick handling in progress, ");
        }

        if (errorBits & (testBit << 20)) {
            errorMessage.append("Incorrect phase rotation direction detected, ");
        }
        if (errorBits & (testBit << 21)) {
            errorMessage.append("No power on mains detected, ");
        }
        if (errorBits >> 22) {
            // Test if there are any bits that are not in the list.
            errorMessage.append("Unknown error, ");
        }
        int stringLength = errorMessage.length();

        // stringLength should be > 1, but just in case.
        if (stringLength > 1) {
            errorMessage.replace(stringLength - 2, 2, "."); // Replace ", " at the end with ".".
        }
        thing->setStateValue(mennekesWallboxErrorMessageStateTypeId, errorMessage);
    }
}
