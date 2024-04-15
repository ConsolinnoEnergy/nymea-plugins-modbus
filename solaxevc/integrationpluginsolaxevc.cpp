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

                ThingDescriptor descriptor(solaxEvcThingClassId, "Solax Wallbox");
                qCInfo(dcSolaxEvc()) << "Discovered:" << descriptor.title() << descriptor.description();

                // Check if we already have set up this device
                Things existingThings = myThings().filterByParam(solaxEvcThingMacAddressParamTypeId, result.networkDeviceInfo.macAddress());
                if (existingThings.count() >= 1) {
                    qCDebug(dcSolaxEvc()) << "This solax inverter already exists in the system:" << result.networkDeviceInfo;
                    descriptor.setThingId(existingThings.first()->id());
                }

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
        // TODO: insert fix from Solax Inverter
        SolaxEvcModbusTcpConnection *connection = new SolaxEvcModbusTcpConnection(monitor->networkDeviceInfo().address(), port, modbusId, this);
        m_tcpConnections.insert(thing, connection);

        // Reconnect on monitor reachable changed
        connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
            qCDebug(dcSolaxEvc()) << "Network device monitor reachable changed for" << thing->name() << reachable;
            if (reachable && !thing->stateValue(solaxEvcConnectedStateTypeId).toBool())
            {
            // TODO: insert fix from Solax inverter
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

        connect(connection, &SolaxEvcModbusTcpConnection::totalPowerChanged, thing, [thing](quint16 totalPower) {
            qCDebug(dcSolaxEvc()) << "Total charging power changed" << totalPower << "W";
            thing->setStateValue(solaxEvcCurrentPowerStateTypeId, totalPower);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::sessionEnergyChanged, thing, [thing](double sessionEnergy) {
            qCDebug(dcSolaxEvc()) << "Session energy changed" << sessionEnergy << "kWh";
            thing->setStateValue(solaxEvcSessionEnergyStateTypeId, sessionEnergy);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::totalEnergyChanged, thing, [thing](double totalEnergy) {
            qCDebug(dcSolaxEvc()) << "Total energy changed" << totalEnergy << "kWh";
            thing->setStateValue(solaxEvcTotalEnergyConsumedStateTypeId, totalEnergy);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::chargingTimeChanged, thing, [thing](quint32 time) {
            qCDebug(dcSolaxEvc()) << "Charging Time changed" << time << "s";
            thing->setStateValue(solaxEvcChargingTimeStateTypeId, time);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::MaxCurrentChanged, thing, [thing](float maxCurrent) {
            qCDebug(dcSolaxEvc()) << "Max current changed" << maxCurrent << "A";
            thing->setStateValue(solaxEvcMaxChargingCurrentStateTypeId, maxCurrent);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::faultCodeChanged, thing, [thing](quint32 code) {
            qCDebug(dcSolaxEvc()) << "Fault code changed" << code;
            QMap<int, QString> faultCodeMap = {{0,"PowerSelect_Fault (0)"},{1,"Emergency Stop (1)"},{2,"Overvoltage L1 (2)"},{3,"Undervoltage L1 (3)"}, \
                                               {4,"Overvoltage L2 (4)"},{5,"Undervoltage L2 (5)"},{6,"Overvoltage L3 (6)"},{7,"Undervoltage L2 (7)"},{8,"Electronic Lock (8)"}, \
                                               {9,"Over Load (9)"},{10,"Over Current (10)"},{11,"Over Temperature (11)"},{12,"PE Ground (12)"},{13,"PE Leak Current (13)"}, \
                                               {14,"Over Leak Current (14)"},{15,"Meter Communication (15)"},{16,"485 Communication (16)"},{17,"CP Voltage (17)"}};
            thing->setStateValue(solaxEvcFaultCodeStateTypeId, faultCodeMap[code]);
        });

        connect(connection, &SolaxEvcModbusTcpConnection::stateChanged, thing, [thing](quint16 state) {
            qCDebug(dcSolaxEvc()) << "State changed" << state;
            // thing->setStateValue(solaxEvcStateStateTypeId, state);

            switch (state)
            {
                case SolaxEvcModbusTcpConnection::StateCharging:
                    thing->setStateValue(solaxEvcChargingStateTypeId, true);
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Charging");
                    break;
                case SolaxEvcModbusTcpConnection::StateFaulted:
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Fault");
                    break;
                case SolaxEvcModbusTcpConnection::StateFinishing:
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, true);
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Plugged In");
                    break;
                default:
                    thing->setStateValue(solaxEvcChargingStateTypeId, false);
                    thing->setStateValue(solaxEvcPluggedInStateTypeId, false);
                    thing->setStateValue(solaxEvcStateStateTypeId, "Available");
                    break;
            }
        });

        connect(connection, &SolaxEvcModbusTcpConnection::chargePhaseChanged, thing, [thing](quint16 phaseCount) {
            qCDebug(dcSolaxEvc()) << "Phase count changed" << phaseCount;
            thing->setStateValue(solaxEvcPhaseCountStateTypeId, phaseCount == 0 ? 1 : 3);
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
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
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
    Q_UNUSED(info)
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
