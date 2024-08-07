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

                    if (foxESSWallbox.macAddress().isNull()) {
                        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The wallbox was found, but the MAC address is invalid. Try searching again."));
                    }
                    Things existingThings = myThings().filterByThingClassId(foxEssThingClassId).filterByParam(foxEssThingMacAddressParamTypeId, foxESSWallbox.macAddress());
                    if (!existingThings.isEmpty())
                        descriptor.setThingId(existingThings.first()->id());

                    ParamList params;
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

    // Initialize if device is reachable again, otherwise set connected state to false
    // and power to 0
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

    // Initialization has finished
    connect(connection, &FoxESSModbusTcpConnection::initializationFinished, thing,
       [this, connection, thing](bool success) {
           thing->setStateValue(foxEssConnectedStateTypeId, success);

           if (success) {
                // Set basic info about device
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

    // Car has been plugged in or unplugged
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

    // Set state for phase current A
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseAChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase A changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseAStateTypeId, current);
    });

    // Set state for phase current B
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseBChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase B changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseBStateTypeId, current);
    });

    // Set state for phase current C
    connect(connection, &FoxESSModbusTcpConnection::currentPhaseCChanged, thing, [thing](float current) {
        qCDebug(dcFoxEss()) << "Current Phase C changed to" << current << "A";
        thing->setStateValue(foxEssCurrentPhaseCStateTypeId, current);
    });

    // Set state for current power
    connect(connection, &FoxESSModbusTcpConnection::currentPowerChanged, thing, [thing](float power) {
        qCDebug(dcFoxEss()) << "Current Power changed to" << power << "kW";
        thing->setStateValue(foxEssCurrentPowerStateTypeId, power*1000);
    });

    // IMPORTANT: as of August 2024, the registers for sessionEnergy and totalConsumedEnergy
    // are mixed up in the data sheet
    // Set state for session energy
    connect(connection, &FoxESSModbusTcpConnection::sessionEnergyConsumedChanged, thing, [thing](float energy) {
        qCDebug(dcFoxEss()) << "Session energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssSessionEnergyStateTypeId, energy);
    });

    // Set state for total consumed energy
    connect(connection, &FoxESSModbusTcpConnection::totalEnergyChanged, thing, [thing](float energy) {
        qCDebug(dcFoxEss()) << "Total energy changed to" << energy << "kWh";
        thing->setStateValue(foxEssTotalEnergyConsumedStateTypeId, energy);
    });

    // Check if alarm info has changed
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

    // Check if fault info has changed
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

    // Check if work mode has changed
    connect(connection, &FoxESSModbusTcpConnection::workModeChanged, [this, thing, connection](quint32 mode) {
        qCDebug(dcFoxEss()) << "Work mode changed to" << mode << ". Make sure it is in controlled mode.";
        // 0x3000 can not be written with FC 0x06
        // Workaround to write work mode register with overwriting maxChargeCurrent
        quint32 maxCurrent = mode & 0x0000FFFF;
        mode = (mode & 0xFFFF0000) >> 16;
        qCDebug(dcFoxEss()) << "Masked work mode:" << mode << "Masked current: " << maxCurrent;
        if (mode != 0)
        {
            qCDebug(dcFoxEss()) << "Setting workmode + maxChargeCurrent to" << (maxCurrent & 0x0000FFFF);
            QModbusReply *reply = connection->setWorkMode(maxCurrent & 0x0000FFFF);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                   qCDebug(dcFoxEss()) << "Successfully send command to set mode to 'controlled mode'";
                }
            });
        }
    });

    // Check if deviceStatus has changed
    connect(connection, &FoxESSModbusTcpConnection::deviceStatusChanged, [this, thing](quint16 state) {
        qCDebug(dcFoxEss()) << "Device status changed to" << state;

        switch (state) {
        case FoxESSModbusTcpConnection::EVCStatusIdle:
            thing->setStateValue(foxEssStateStateTypeId, "Available");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusConnected:
            thing->setStateValue(foxEssStateStateTypeId, "Connected");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusStarting:
            thing->setStateValue(foxEssStateStateTypeId, "Starting");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusCharging:
            thing->setStateValue(foxEssStateStateTypeId, "Charging");
            thing->setStateValue(foxEssChargingStateTypeId, true);
            break;
        case FoxESSModbusTcpConnection::EVCStatusPausing:
            thing->setStateValue(foxEssStateStateTypeId, "Paused");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFinishing:
            thing->setStateValue(foxEssStateStateTypeId, "Finished");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        case FoxESSModbusTcpConnection::EVCStatusFaulted:
            thing->setStateValue(foxEssStateStateTypeId, "Faulted");
            thing->setStateValue(foxEssChargingStateTypeId, false);
            break;
        }
    });

    connect(m_chargeLimitTimer, &QTimer::timeout, this, [this, thing, connection]() {
        qCDebug(dcFoxEss()) << "m_chargeLimitTimer timeout.";
        float currentInApp = thing->stateValue(foxEssMaxChargingCurrentStateTypeId).toFloat();
        int phaseCount = thing->stateValue(foxEssPhaseCountStateTypeId).toUInt();
        setMaxCurrent(connection, currentInApp, phaseCount);
    });

    // Check if update has finished
    connect(connection, &FoxESSModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
        qCDebug(dcFoxEss()) << "Update finished";
        float currentPhaseA = thing->stateValue(foxEssCurrentPhaseAStateTypeId).toFloat();
        float currentPhaseB = thing->stateValue(foxEssCurrentPhaseBStateTypeId).toFloat();
        float currentPhaseC = thing->stateValue(foxEssCurrentPhaseCStateTypeId).toFloat();

        // Set state count
        int phaseCount = 0;
        if (currentPhaseA > 0)
            phaseCount++;
        if (currentPhaseB > 0)
            phaseCount++;
        if (currentPhaseC > 0)
            phaseCount++;
        thing->setStateValue(foxEssPhaseCountStateTypeId, qMax(phaseCount,1));

        // TODO: Test working
        // If state is Charging, but power is false -> stop charging - DONE
        // if state available, connected, starting and power is true -> start charging - DONE
        QString state = thing->stateValue(foxEssStateStateTypeId).toString();
        bool power = thing->stateValue(foxEssPowerStateTypeId).toBool();
        if ((state == "Charging") && (power == false)) 
        {
            toggleCharging(connection, false);
        }

        if ((power == true) && ((state == "Available") ||
                                (state == "Connected") ||
                                (state == "Starting")  ||
                                (state == "Finished")))
        {
            toggleCharging(connection, true);
        }
        
        if (state == "Charging") {
            if (!m_chargeLimitTimer->isActive()) {
                qCDebug(dcFoxEss()) << "State is charging; Starting m_chargeLimitTimer.";
                m_chargeLimitTimer->start();
            }
        } else {
            if (m_chargeLimitTimer->isActive()) {
                qCDebug(dcFoxEss()) << "State is not charging; Stopping m_chargeLimitTimer.";
                m_chargeLimitTimer->stop();
            }
        }

        // Make current of wallbox follow app
        if ((state == "Charging") && (power == true)) {
            qCDebug(dcFoxEss()) << "Charging, but current is not corret. Correcting";
            double meanCurrent = (currentPhaseA + currentPhaseB + currentPhaseC) / phaseCount;
            float currentInApp = thing->stateValue(foxEssMaxChargingCurrentStateTypeId).toFloat();
            if ((meanCurrent > currentInApp+2) || (meanCurrent < currentInApp-2)) {
                setMaxCurrent(connection, currentInApp, phaseCount);
            }
        }
    });
    
    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginFoxEss::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcFoxEss()) << "Post setup thing..";

    if (!m_chargeLimitTimer)
        m_chargeLimitTimer = new QTimer(this);
    m_chargeLimitTimer->setInterval(45*1000);

    if (!m_pluginTimer) {
        qCDebug(dcFoxEss()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
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
            thing->setStateValue(foxEssPowerStateTypeId, power);
            info->finish(Thing::ThingErrorNoError);
        }

        if (info->action().actionTypeId() == foxEssMaxChargingCurrentActionTypeId) {
            float maxCurrent = info->action().paramValue(foxEssMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toFloat();
            int phaseCount = thing->stateValue(foxEssPhaseCountStateTypeId).toUInt();
            thing->setStateValue(foxEssMaxChargingCurrentStateTypeId, maxCurrent);
            setMaxCurrent(connection, maxCurrent, phaseCount);
            info->finish(Thing::ThingErrorNoError);
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
            qCDebug(dcFoxEss()) << "Toggle charge was not sent successfully";
            // toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginFoxEss::setMaxCurrent(FoxESSModbusTcpConnection *connection, float maxCurrent, int phaseCount)
{
    float maxPower = connection->maxChargePower();
    qCDebug(dcFoxEss()) << "Setting maxChargeCurrent to" << maxCurrent;
    qCDebug(dcFoxEss()) << "Setting maxChargePower to" << maxPower;
    if (maxCurrent < 6)
        maxCurrent = 7;
    maxPower = (230 * phaseCount * maxCurrent) / 1000;
    qCDebug(dcFoxEss()) << "Calculated power is" << maxPower;
    maxCurrent = maxPower;
    QModbusReply *reply = connection->setMaxChargeCurrent(maxCurrent, maxPower);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, maxCurrent, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcFoxEss()) << "Successfully set maximum charging current";
        } else {
            qCDebug(dcFoxEss()) << "Setting max current was not successfull";
            // setMaxCurrent(connection, maxCurrent);
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

    if (myThings().isEmpty() && m_chargeLimitTimer) {
        m_chargeLimitTimer->stop();
        delete m_chargeLimitTimer;
        m_chargeLimitTimer = nullptr;
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}
