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

#include "plugininfo.h"
#include "integrationpluginschneider.h"


IntegrationPluginSchneider::IntegrationPluginSchneider()
{

}

void IntegrationPluginSchneider::init()
{

}

void IntegrationPluginSchneider::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcSchneiderElectric()) << "The network discovery does not seem to be available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("The discovery is not available. Please enter the IP address manually."));
        return;
    }

    if (info->thingClassId() == schneiderEVlinkThingClassId) {

        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
            foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {

                qCDebug(dcSchneiderElectric()) << "Found" << networkDeviceInfo;

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

                ThingDescriptor descriptor(schneiderEVlinkThingClassId, title, description);
                ParamList params;
                params << Param(schneiderEVlinkThingIpParamTypeId, networkDeviceInfo.address().toString());
                params << Param(schneiderEVlinkThingMacParamTypeId, networkDeviceInfo.macAddress());
                descriptor.setParams(params);

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(schneiderEVlinkThingIpParamTypeId, networkDeviceInfo.macAddress());
                if (existingThings.count() == 1) {
                    qCDebug(dcSchneiderElectric()) << "This connection already exists in the system:" << networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

                info->addThingDescriptor(descriptor);
            }
            info->finish(Thing::ThingErrorNoError);
        });
    } else {
        qCWarning(dcSchneiderElectric()) << "Could not discover things because of unhandled thing class id" << info->thingClassId().toString();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}


void IntegrationPluginSchneider::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == schneiderEVlinkThingClassId) {

        // Handle reconfigure
        if (myThings().contains(thing)) {
            SchneiderWallbox *schneiderWallbox = m_schneiderDevices.take(thing->id());
            if (schneiderWallbox) {
                qCDebug(dcSchneiderElectric()) << "Reconfigure" << thing->name() << thing->params();
                delete schneiderWallbox;
                // Now continue with the normal setup
            }
        }

        qCDebug(dcSchneiderElectric()) << "Setup" << thing << thing->params();

        QHostAddress hostAddress = QHostAddress(thing->paramValue(schneiderEVlinkThingIpParamTypeId).toString());
        if (hostAddress.isNull()){
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("No IP address given"));
            return;
        }

        uint port = thing->paramValue(schneiderEVlinkThingPortParamTypeId).toUInt();
        quint16 slaveId = thing->paramValue(schneiderEVlinkThingSlaveIdParamTypeId).toUInt();

        quint16 minCurrentLimit = thing->paramValue(schneiderEVlinkThingMinChargeCurrentParamTypeId).toUInt();
        quint16 maxCurrentLimit = thing->paramValue(schneiderEVlinkThingMaxChargeCurrentParamTypeId).toUInt();
        thing->setStateMinMaxValues(schneiderEVlinkMaxChargingCurrentStateTypeId, minCurrentLimit, maxCurrentLimit);

        // ToDo: implement code to update the wallbox config file so that minimum current for mono and tri phase charging is the same.

        // Check if we have a schneiderWallbox with this ip and slaveId, if reconfigure the object would already been removed from the hash
        foreach (SchneiderWallbox *schneiderWallboxEntry, m_schneiderDevices.values()) {
            if (schneiderWallboxEntry->modbusTcpConnection()->hostAddress() == hostAddress && schneiderWallboxEntry->slaveId() == slaveId) {
                qCWarning(dcSchneiderElectric()) << "Failed to set up schneiderWallbox for host address" << hostAddress.toString() << "and slaveId" << slaveId << "because there has already been configured a schneiderWallbox for this IP and slaveId.";
                info->finish(Thing::ThingErrorThingInUse, QT_TR_NOOP("Already configured for this IP address and slaveId."));
                return;
            }
        }

        // ToDO: Check if this is really a Schneider wallbox.

        SchneiderModbusTcpConnection *schneiderConnectTcpConnection = new SchneiderModbusTcpConnection(hostAddress, port, slaveId, this);
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::connectionStateChanged, this, [thing, schneiderConnectTcpConnection](bool status){
            qCDebug(dcSchneiderElectric()) << "Connected changed to" << status << "for" << thing;
            if (status) {
                schneiderConnectTcpConnection->update();
            }

            thing->setStateValue(schneiderEVlinkConnectedStateTypeId, status);
        });

        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::chargeTimeChanged, this, [thing](quint32 chargeTime){
            quint32 chargeTimeMinutes{chargeTime / 60};
            qCDebug(dcSchneiderElectric()) << thing << "charging time changed" << chargeTimeMinutes << "Minutes";
            thing->setStateValue(schneiderEVlinkChargeTimeStateTypeId, chargeTimeMinutes);
        });
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::cpwStateChanged, this, [thing, this](SchneiderModbusTcpConnection::CPWState cpwState){
            qCDebug(dcSchneiderElectric()) << thing << "CPW state changed" << cpwState ;
            setCpwState(thing, cpwState);
        });
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::lastChargeStatusChanged, this, [thing, this](SchneiderModbusTcpConnection::LastChargeStatus lastChargeStatus){
            qCDebug(dcSchneiderElectric()) << thing << "Last charge status changed" << lastChargeStatus ;
            setLastChargeStatus(thing, lastChargeStatus);
        });
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::stationEnergyChanged, this, [thing](quint32 stationEnergyInKilowatt){
            double stationEnergyInKilowattHours{(static_cast<double>(stationEnergyInKilowatt)) / 1000};
            qCDebug(dcSchneiderElectric()) << thing << "station energy changed" << stationEnergyInKilowattHours << "kWh";
            thing->setStateValue(schneiderEVlinkTotalEnergyConsumedStateTypeId, stationEnergyInKilowattHours);
        });
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::stationPowerTotalChanged, this, [thing](float stationPowerTotalKilowatt){
            double stationPowerTotalWatt{stationPowerTotalKilowatt * 1000};
            qCDebug(dcSchneiderElectric()) << thing << "station total power changed" << stationPowerTotalWatt << "W";
            thing->setStateValue(schneiderEVlinkCurrentPowerStateTypeId, stationPowerTotalWatt);
        });
        connect(schneiderConnectTcpConnection, &SchneiderModbusTcpConnection::errorStatusChanged, this, [thing, this](quint32 errorBits){
            qCDebug(dcSchneiderElectric()) << thing << "Error state changed" << errorBits ;
            setErrorMessage(thing, errorBits);
        });

        schneiderConnectTcpConnection->connectDevice();

        SchneiderWallbox *schneiderWallbox = new SchneiderWallbox(schneiderConnectTcpConnection, slaveId, this);
        int ampereValue = thing->stateValue(schneiderEVlinkMaxChargingCurrentStateTypeId).toUInt();
        bool success = schneiderWallbox->setMaxAmpere(ampereValue);
        if (!success) {
            qCWarning(dcSchneiderElectric()) << "Could not set max charging current to initial value.";
        }

        connect(schneiderWallbox, &SchneiderWallbox::phaseCountChanged, this, [thing, this](quint16 phaseCount){
            qCDebug(dcSchneiderElectric()) << thing << "Phase count changed" << phaseCount ;
            setPhaseCount(thing, phaseCount);
        });
        connect(schneiderWallbox, &SchneiderWallbox::currentPowerChanged, this, [thing, this](double currentPower){
            qCDebug(dcSchneiderElectric()) << thing << "Current power changed" << currentPower ;
            setCurrentPower(thing, currentPower);
        });

        m_schneiderDevices.insert(thing->id(), schneiderWallbox);

        connect(info, &ThingSetupInfo::aborted, schneiderWallbox, &SchneiderWallbox::deleteLater); // Clean up if the setup fails
        connect(schneiderWallbox, &SchneiderWallbox::destroyed, this, [thing, this]{
            m_schneiderDevices.remove(thing->id());
            //Todo: Setup failed, lets search the network, maybe the IP has changed...
        });


        info->finish(Thing::ThingErrorNoError);
    } else {
        qCWarning(dcSchneiderElectric()) << "Could not setup thing: unhandled device class" << thing->thingClass();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}


void IntegrationPluginSchneider::postSetupThing(Thing *thing)
{
    qCDebug(dcSchneiderElectric()) << "Post setup" << thing->name();
    if (thing->thingClassId() != schneiderEVlinkThingClassId) {
        qCWarning(dcSchneiderElectric()) << "Thing class id not supported" << thing->thingClassId();
        return;
    }

    if (!m_pluginTimer) {
        qCDebug(dcSchneiderElectric()) << "Starting plugin timer...";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            foreach(SchneiderWallbox *device, m_schneiderDevices) {
                if (device->modbusTcpConnection()->connected()) {
                    device->modbusTcpConnection()->update();
                    device->update();
                }
            }
        });

        m_pluginTimer->start();
    }
}

void IntegrationPluginSchneider::thingRemoved(Thing *thing)
{
    if (thing->thingClassId() == schneiderEVlinkThingClassId && m_schneiderDevices.contains(thing->id())) {
        qCDebug(dcSchneiderElectric()) << "Deleting" << thing->name();

        // ToDo: set charging to off? Delete code in schneiderwallbox.cpp has code to turn charging off. Not sure if that works though.

        SchneiderWallbox *device = m_schneiderDevices.take(thing->id());
        device->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        qCDebug(dcSchneiderElectric()) << "Stopping plugin timers ...";
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginSchneider::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();


    if (thing->thingClassId() == schneiderEVlinkThingClassId) {
        SchneiderWallbox *device = m_schneiderDevices.value(thing->id());

        if (!device->modbusTcpConnection()->connected()) {
            qCWarning(dcSchneiderElectric()) << "Could not execute action. The modbus connection is currently not available.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        bool success = false;
        if (action.actionTypeId() == schneiderEVlinkPowerActionTypeId) {
            bool onOff = action.paramValue(schneiderEVlinkPowerActionPowerParamTypeId).toBool();
            success = device->enableOutput(onOff);
            info->thing()->setStateValue(schneiderEVlinkPowerStateTypeId, onOff);
        } else if(action.actionTypeId() == schneiderEVlinkMaxChargingCurrentActionTypeId) {
            int ampereValue = action.paramValue(schneiderEVlinkMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toUInt();
            success = device->setMaxAmpere(ampereValue);
            info->thing()->setStateValue(schneiderEVlinkMaxChargingCurrentStateTypeId, ampereValue);
        } else {
            qCWarning(dcSchneiderElectric()) << "Unhandled ActionTypeId:" << action.actionTypeId();
            return info->finish(Thing::ThingErrorActionTypeNotFound);
        }

        if (success) {
            info->finish(Thing::ThingErrorNoError);
        } else {
            qCWarning(dcSchneiderElectric()) << "Action execution finished with error.";
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }
    } else {
        qCWarning(dcSchneiderElectric()) << "Execute action, unhandled device class" << thing->thingClass();
        info->finish(Thing::ThingErrorThingClassNotFound);
    }
}

void IntegrationPluginSchneider::setCpwState(Thing *thing, SchneiderModbusTcpConnection::CPWState state)
{
    bool isCharging{false};
    switch (state) {
    case SchneiderModbusTcpConnection::CPWStateEvseNotAvailable:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EVSE not available");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvseAvailable:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EVSE available");
        break;
    case SchneiderModbusTcpConnection::CPWStatePlugDetected:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Plug detected");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvConnected:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EV connected");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvConnected2:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EV connected 2");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvConnectedVentilationRequired:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EV connected ventilation required");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvseReady:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EVSE ready");
        break;
    case SchneiderModbusTcpConnection::CPWStateEvReady:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EV ready");
        break;
    case SchneiderModbusTcpConnection::CPWStateCharging:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Charging");
        isCharging = true;
        break;
    case SchneiderModbusTcpConnection::CPWStateEvReadyVentilationRequired:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EV ready ventilation required");
        break;
    case SchneiderModbusTcpConnection::CPWStateChargingVentilationRequired:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Charging ventilation required");
        break;
    case SchneiderModbusTcpConnection::CPWStateStopCharging:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Stop charging");
        break;
    case SchneiderModbusTcpConnection::CPWStateAlarm:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Alarm");
        break;
    case SchneiderModbusTcpConnection::CPWStateShortcut:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Short circuit");
        break;
    case SchneiderModbusTcpConnection::CPWStateDigitalComByEvseState:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "Digital com by EVSE state");
        break;
    default:
        thing->setStateValue(schneiderEVlinkCpwStateStateTypeId, "EVSE not available");
    }
    if (isCharging) {
        thing->setStateValue(schneiderEVlinkChargingStateTypeId, true);
    } else {
        thing->setStateValue(schneiderEVlinkChargingStateTypeId, false);
    }
}

void IntegrationPluginSchneider::setLastChargeStatus(Thing *thing, SchneiderModbusTcpConnection::LastChargeStatus status)
{
    switch (status) {
    case SchneiderModbusTcpConnection::LastChargeStatusCircuitBreakerEnabled:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Circuit breaker enabled (emergency)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusOk:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "OK (ended by EV in mode 3)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusEndedByClusterManagerLoss:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Ended by cluster manager loss (lifebit timeout 10s)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusEndOfChargeInSm3:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "End of charge in SM3 (low current)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusCommunicationError:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Communication error (PWM lost)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDisconnectionCable:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Disconnection cable (Electric Vehicle Supply Equipment plug off)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDisconnectionEv:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Disconnection EV (EV plug off)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusShortcut:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Shortcut");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusOverload:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Overload");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusCanceledBySupervisor:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Canceled by supervisor (external command)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusVentilationNotAllowed:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Ventillation not allowed");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusUnexpectedContactorOpen:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Unexpected contactor open");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusSimplifiedMode3NotAllowed:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Simplified mode 3 not allowed");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusPowerSupplyInternalError:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Power supply internal error(contactor not able to close)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusUnexpectedPlugUnlock:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Unexpected plug unlock");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDefaultNbPhases:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Default Nb Phases (triphase not allowed)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultSurgeArrestor:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default Surge arrestor");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultAntiIntrusion:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default Anti Intrusion");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultShutterUnlock:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default Shutter Unlock");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultFlsi:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default FLSI (Force Load Shedding Input)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultEmergencyStop:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default Emergency Stop");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultUndervoltage:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default Undervoltage");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusDiDefaultCi:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "DI default CI (Conditional Input)");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusOther:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Other");
        break;
    case SchneiderModbusTcpConnection::LastChargeStatusUndefined:
    default:
        thing->setStateValue(schneiderEVlinkLastChargeStatusStateTypeId, "Undefined");
    }
}

void IntegrationPluginSchneider::setCurrentPower(Thing *thing, double currentPower) {
    thing->setStateValue(schneiderEVlinkCurrentPowerStateTypeId, currentPower);
}

void IntegrationPluginSchneider::setPhaseCount(Thing *thing, quint16 phaseCount) {
    thing->setStateValue(schneiderEVlinkPhaseCountStateTypeId, phaseCount);
    //qCDebug(dcSchneiderElectric()) << "Phase count set:" << phaseCount << " for thing:" << thing;
}

void IntegrationPluginSchneider::setErrorMessage(Thing *thing, quint32 errorBits) {
    if (!errorBits) {
        thing->setStateValue(schneiderEVlinkErrorMessageStateTypeId, "No error");
    } else {
        QString errorMessage{""};
        quint32 testBit{0b01};
        if (errorBits & testBit) {
            errorMessage.append("Lost communication with RFID reader, ");
        }
        if (errorBits & (testBit << 1)) {
            errorMessage.append("Lost communication with display, ");
        }
        if (errorBits & (testBit << 2)) {
            errorMessage.append("Cannot connect to master board, ");
        }
        if (errorBits & (testBit << 3)) {
            errorMessage.append("Incorrect plug lock state, ");
        }
        if (errorBits & (testBit << 4)) {
            errorMessage.append("Incorrect contactor state, ");
        }
        if (errorBits & (testBit << 5)) {
            errorMessage.append("Incorrect surge arrestor state, ");
        }
        if (errorBits & (testBit << 6)) {
            errorMessage.append("Incorrect anti intrusion state, ");
        }
        if (errorBits & (testBit << 7)) {
            errorMessage.append("Cannot connect to US daughter board, ");
        }
        if (errorBits & (testBit << 8)) {
            errorMessage.append("Configuration file missing, corrupted or already open, ");
        }
        if (errorBits & (testBit << 9)) {
            errorMessage.append("Incorrect shutter lock state, ");
        }
        if (errorBits & (testBit << 10)) {
            errorMessage.append("Incorrect circuit breaker state, ");
        }
        if (errorBits & (testBit << 11)) {
            errorMessage.append("Lost communication with powermeter, ");
        }
        if (errorBits & (testBit << 12)) {
            errorMessage.append("remote controller lost, ");
        }
        if (errorBits & (testBit << 13)) {
            errorMessage.append("Incorrect socket state, ");
        }
        if (errorBits & (testBit << 14)) {
            errorMessage.append("Incorrect charging phase number, ");
        }
        if (errorBits & (testBit << 15)) {
            errorMessage.append("Lost communication with cluster manager, ");
        }
        if (errorBits & (testBit << 16)) {
            errorMessage.append("Mode3 communication error (CP error), ");
        }

        if (errorBits & (testBit << 17)) {
            errorMessage.append("Incorrect cable state (PP error), ");
        }

        if (errorBits & (testBit << 18)) {
            errorMessage.append("Default EV charging cable disconnection, ");
        }

        if (errorBits & (testBit << 19)) {
            errorMessage.append("Short circuit FP1, ");
        }

        if (errorBits & (testBit << 20)) {
            errorMessage.append("Overcurrent, ");
        }
        if (errorBits & (testBit << 21)) {
            errorMessage.append("No energy available for charging, ");
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
        thing->setStateValue(schneiderEVlinkErrorMessageStateTypeId, errorMessage);
    }
}
