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

#include "integrationpluginfoxesswbv3.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include "platform/platformzeroconfcontroller.h"
#include "network/zeroconf/zeroconfservicebrowser.h"

IntegrationPluginFoxEss::IntegrationPluginFoxEss() { }

void IntegrationPluginFoxEss::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcFoxEss()) << "FoxESS - Discovery started";
    if (info->thingClassId() == foxEssThingClassId) {
        if (!hardwareManager()->zeroConfController()->available())
        {
            qCWarning(dcFoxEss()) << "Error discovering FoxESS wallbox. Available:"  << hardwareManager()->zeroConfController()->available();
            info->finish(Thing::ThingErrorHardwareNotAvailable, "Thing discovery not possible");
            return;
        }
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcFoxEss()) << "Discovery: Network discovery finished. Found"
                              << discoveryReply->networkDeviceInfos().count() << "network devices";
            m_serviceBrowser = hardwareManager()->zeroConfController()->createServiceBrowser();
            foreach (const ZeroConfServiceEntry &service, m_serviceBrowser->serviceEntries()) {
                // qCDebug(dcFoxEss()) << "mDNS service entry:" << service;
                if (service.hostName().contains("EVC-"))
                {
                    ThingDescriptor descriptor(foxEssThingClassId, "FoxESS Wallbox", service.hostAddress().toString());
                    NetworkDeviceInfo foxESSWallbox = discoveryReply->networkDeviceInfos().get(service.hostAddress());
                    qCDebug(dcFoxEss()) << "MacAddress of WB:" << foxESSWallbox.macAddress();
                    ParamList params;
                    // TODO: Get Mac Address
                    params << Param(foxEssThingIpAddressParamTypeId, service.hostAddress().toString());
                    params << Param(foxEssThingMdnsNameParamTypeId, service.name());
                    params << Param(foxEssThingPortParamTypeId, service.port());
                    params << Param(foxEssThingMacAddressParamTypeId, foxESSWallbox.macAddress());
                    params << Param(foxEssThingModbusIdParamTypeId, 1);
                    descriptor.setParams(params);
                    info->addThingDescriptor(descriptor);
                }
            }
            discoveryReply->deleteLater();
            info->finish(Thing::ThingErrorNoError);
        });
    }
}

void IntegrationPluginFoxEss::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcFoxEss()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == foxEssThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress =
                MacAddress(thing->paramValue(foxEssThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcFoxEss())
                    << "Failed to set up FoxESS wallbox because the MAC address is not valid:"
                    << thing->paramValue(foxEssThingMacAddressParamTypeId).toString()
                    << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter,
                         QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device "
                                    "to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is
        // reachable without polling on our own. In this call, nymea is checking a list for known
        // mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor =
                hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip
        // address and is set to not reachable
        m_monitors.insert(thing, monitor);
    
        qCDebug(dcFoxEss()) << "Monitor reachable" << monitor->reachable()
                              << thing->paramValue(foxEssThingMacAddressParamTypeId).toString();
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(hardwareManager()->networkDeviceDiscovery(),
                &NetworkDeviceDiscovery::cacheUpdated, info, [this, info]() {
                    if (!m_setupTcpConnectionRunning) {
                        m_setupTcpConnectionRunning = true;
                        setupTcpConnection(info);
                    }
            });
        }
    }
}

void IntegrationPluginFoxEss::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcFoxEss()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    uint port = thing->paramValue(foxEssThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(foxEssThingModbusIdParamTypeId).toUInt();
    FoxESSModbusTcpConnection *connection = new FoxESSModbusTcpConnection(
            monitor->networkDeviceInfo().address(), port, modbusId, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcFoxEss()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcFoxEss()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(foxEssConnectedStateTypeId).toBool()) {
            connection->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::reachableChanged, thing,
            [this, connection, thing](bool reachable) {
                qCDebug(dcFoxEss()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(foxEssConnectedStateTypeId, false);
                    thing->setStateValue(foxEssCurrentPowerStateTypeId, 0);
                }
    });

    connect(connection, &FoxESSModbusTcpConnection::initializationFinished, thing,
       [this, connection, thing](bool success) {
           thing->setStateValue(foxEssConnectedStateTypeId, success);

           if (success) {
               qCDebug(dcFoxEss()) << "FoxESS wallbox initialized.";
               quint16 firmware = connection->firmwareVersion();
               QString majorVersion = QString::number((firmware & 0xFF00) >> 8);
               QString minorVersion = QString::number(firmware & 0xFF);
               thing->setStateValue(foxEssFirmwareVersionStateTypeId,
                                    majorVersion+"."+minorVersion);
               thing->setStateValue(foxEssSerialNumberStateTypeId,
                                    connection->serialNumber());

               qCDebug(dcFoxEss()) << "Max Supported Power is" << connection->maxSupportedPower() << "kW";
               if (connection->maxSupportedPower() == 11) {
                   qCDebug(dcFoxEss()) << "Changed max current to 16"; 
                   thing->setStateMaxValue(foxEssMaxChargingCurrentStateTypeId, 16);
               } else {
                   qCDebug(dcFoxEss()) << "Changed max current to 32"; 
                   thing->setStateMaxValue(foxEssMaxChargingCurrentStateTypeId, 32);
               }
           } else {
               qCDebug(dcFoxEss()) << "FoxESS wallbox initialization failed.";
               // Try to reconnect to device
               connection->reconnectDevice();
           }
    });

    connect(connection, &FoxESSModbusTcpConnection::plugStatusChanged, thing, [thing](quint16 state) {
        if (state == 0)
        {
            // not connected
            qCDebug(dcFoxEss()) << "Plug not connected";
            thing->setStateValue(foxEssPluggedInStateTypeId, false);
        } else {
            // connected
            qCDebug(dcFoxEss()) << "Plug connected";
            thing->setStateValue(foxEssPluggedInStateTypeId, true);
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::currentPhaseAChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase A changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseAStateTypeId, current);
    });

    connect(connection, &FoxESSModbusTcpConnection::currentPhaseBChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase B changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseBStateTypeId, current);
    });

    connect(connection, &FoxESSModbusTcpConnection::currentPhaseCChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase C changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseCStateTypeId, current);
    });

    connect(connection, &FoxESSModbusTcpConnection::currentPowerChanged, thing, [thing](float power) {
        qCDebug(dcFoxEss()) << "Current Power changed to" << power << "kW";
        thing->setStateValue(foxEssCurrentPowerStateTypeId, power*1000);
    });

    connect(connection, &FoxESSModbusTcpConnection::sessionEnergyConsumedChanged, thing, [thing](float energy) {
        // TODO: Session Energy und Total Energy evtl vertauscht
        qCDebug(dcFoxEss()) << "Session energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssSessionEnergyStateTypeId, energy);
    });

    connect(connection, &FoxESSModbusTcpConnection::totalEnergyChanged, thing, [thing](float energy) {
        qCDebug(dcFoxEss()) << "Total energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssTotalEnergyConsumedStateTypeId, energy);
    });

    connect(connection, &FoxESSModbusTcpConnection::alarmInfoChanged, thing, [thing](quint16 code) {
        qCDebug(dcFoxEss()) << "Alarm info changed to" << code;
        QMap<int, QString> alarmInfoMap = {
            {0, "Card reader"}, {1, "Phase cutting box"},
            {2, "Phase loss"}
        };

        QString warningMessage = "";
        for (int i = 0; i <= 2; i++)
        {
            if (((code >> i) & 0x1) == 0x1)
            {
                warningMessage.append(alarmInfoMap[i]);
            }
        }
        qCDebug(dcFoxEss()) << "Alaram code:" << code << "Alarm info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(foxEssAlarmInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(foxEssAlarmInfoStateTypeId, "No Error");
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::faultInfoChanged, thing, [thing](quint32 code) {
        qCDebug(dcFoxEss()) << "Fault info changed to" << code;
        QMap<int, QString> faultInfoMap = {
            {0, "Emergency stop"}, {1, "Overvoltage"},
            {2, "Undervoltage"}, {3, "Overcurrent"},
            {4, "Charge port temperature"}, {5, "PE grounding"},
            {6, "Leakage current"}, {7, "Frequency"},
            {8, "CP"}, {9, "Connector"},
            {10, "AC contactor"}, {11, "Electronic lock"},
            {12, "Breaker"}, {13, "CC"},
            {14, "Ext. meter communiction"}, {15, "Metering chip"},
            {16, "Environment temperature"}, {17, "Access control"},
        };
        QString warningMessage = "";
        for (int i = 0; i <= 17; i++)
        {
            if (((code >> i) & 0x1) == 0x1)
            {
                warningMessage.append(faultInfoMap[i]);
            }
        }
        qCDebug(dcFoxEss()) << "Fault code:" << code << "Fault info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(foxEssFaultInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(foxEssFaultInfoStateTypeId, "No Error");
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::stopReasonChanged, [this, thing](quint16 value) {
        qCDebug(dcFoxEss()) << "Stop Reason changed to" << value;
        // TODO: Map mit werten + Text erstllen und setzen
    });

    connect(connection, &FoxESSModbusTcpConnection::workModeChanged, [this, thing, connection](quint16 mode) {
        qCDebug(dcFoxEss()) << "Work mode changed to" << mode << ". Make sure it is in controlled mode.";
        if (mode != 0)
        {
            QModbusReply *reply = connection->setWorkMode(0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                   qCDebug(dcFoxEss()) << "Successfully send command to set mode to 'controlled mode'";
                }
            });
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::deviceStatusChanged, [this, thing](quint16 state) {
        qCDebug(dcFoxEss()) << "Device status changed to" << state;

        switch (state) {
        case FoxESSModbusTcpConnection::EVCStatusIdle:
            thing->setStateValue(foxEssStateStateTypeId, "Available");
            // thing->setStateValue(foxEssPluggedInStateTypeId, false);
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusConnected:
            thing->setStateValue(foxEssStateStateTypeId, "Connected");
            // thing->setStateValue(foxEssPluggedInStateTypeId, true);
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusStarting:
            thing->setStateValue(foxEssStateStateTypeId, "Starting");
            // thing->setStateValue(foxEssPluggedInStateTypeId, true);
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusCharging:
            thing->setStateValue(foxEssStateStateTypeId, "Charging");
            // thing->setStateValue(foxEssPluggedInStateTypeId, true);
            thing->setStateValue(foxEssChargingStateTypeId, true);
            break;
        case FoxESSModbusTcpConnection::EVCStatusPausing:
            thing->setStateValue(foxEssStateStateTypeId, "Paused");
            // thing->setStateValue(foxEssPluggedInStateTypeId, true);
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFinishing:
            thing->setStateValue(foxEssStateStateTypeId, "Finished");
            // thing->setStateValue(foxEssPluggedInStateTypeId, false);
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFaulted:
            thing->setStateValue(foxEssStateStateTypeId, "Faulted");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        }
    });

    connect(connection, &FoxESSModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
        qCDebug(dcFoxEss()) << "Update finished";
        float currentPhaseA = thing->stateValue(foxEssCurrentPhaseAStateTypeId).toFloat();
        float currentPhaseB = thing->stateValue(foxEssCurrentPhaseBStateTypeId).toFloat();
        float currentPhaseC = thing->stateValue(foxEssCurrentPhaseCStateTypeId).toFloat();

        int phaseCount = 0;
        if (currentPhaseA > 0)
            phaseCount++;
        if (currentPhaseB > 0)
            phaseCount++;
        if (currentPhaseC > 0)
            phaseCount++;
        thing->setStateValue(foxEssPhaseCountStateTypeId, qMax(phaseCount,1));
    });
    
    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginFoxEss::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcFoxEss()) << "Post setup thing..";

    if (!m_pluginTimer) {
        qCDebug(dcFoxEss()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcFoxEss()) << "Updating FoxESS EVC..";
            foreach (FoxESSModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginFoxEss::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == foxEssThingClassId)
    {
        FoxESSModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (info->action().actionTypeId() == foxEssPowerActionTypeId)
        {
            qCDebug(dcFoxEss()) << "Start / Stop charging";
            bool power = info->action().paramValue(foxEssPowerActionPowerParamTypeId).toBool();
            toggleCharging(connection, power);
            info->finish(Thing::ThingErrorNoError);
            thing->setStateValue(foxEssPowerStateTypeId, power);
        }
    }
}

void IntegrationPluginFoxEss::toggleCharging(FoxESSModbusTcpConnection *connection, bool power)
{
    quint16 startCharging = 1;
    quint16 stopCharging = 2;
    QModbusReply *reply = connection->setChargingControl(power ? startCharging : stopCharging);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, power, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcFoxEss()) << "Successfully set charge control";
        } else {
            toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginFoxEss::thingRemoved(Thing *thing)
{
    qCDebug(dcFoxEss()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        FoxESSModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
