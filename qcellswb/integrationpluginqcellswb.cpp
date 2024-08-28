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

#include "integrationpluginqcellswb.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>
#include "platform/platformzeroconfcontroller.h"
#include "network/zeroconf/zeroconfservicebrowser.h"

IntegrationPluginQCells::IntegrationPluginQCells() { }

void IntegrationPluginQCells::discoverThings(ThingDiscoveryInfo *info)
{
    qCDebug(dcQcells()) << "QCells - Discovery started";
    if (info->thingClassId() == qCellsThingClassId) {
        if (!hardwareManager()->zeroConfController()->available())
        {
            qCWarning(dcQcells()) << "Error discovering QCells wallbox. Available:"  << hardwareManager()->zeroConfController()->available();
            info->finish(Thing::ThingErrorHardwareNotAvailable, "Thing discovery not possible");
            return;
        }
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcQcells()) << "Discovery: Network discovery finished. Found"
                              << discoveryReply->networkDeviceInfos().count() << "network devices";
            m_serviceBrowser = hardwareManager()->zeroConfController()->createServiceBrowser();
            foreach (const ZeroConfServiceEntry &service, m_serviceBrowser->serviceEntries()) {
                // qCDebug(dcQcells()) << "mDNS service entry:" << service;
                if (service.hostName().contains("EVC-"))
                {
                    ThingDescriptor descriptor(qCellsThingClassId, "QCells Wallbox", service.hostAddress().toString());
                    NetworkDeviceInfo qcellsWallbox = discoveryReply->networkDeviceInfos().get(service.hostAddress());
                    qCDebug(dcQcells()) << "MacAddress of WB:" << qcellsWallbox.macAddress();

                    if (service.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol) {
                        continue;
                    }

                    if (qcellsWallbox.macAddress().isNull()) {
                        info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The wallbox was found, but the MAC address is invalid. Try searching again."));
                    }
                    Things existingThings = myThings().filterByThingClassId(qCellsThingClassId).filterByParam(qCellsThingMacAddressParamTypeId, qcellsWallbox.macAddress());
                    if (!existingThings.isEmpty())
                        descriptor.setThingId(existingThings.first()->id());

                    ParamList params;
                    params << Param(qCellsThingIpAddressParamTypeId, service.hostAddress().toString());
                    params << Param(qCellsThingMdnsNameParamTypeId, service.name());
                    params << Param(qCellsThingPortParamTypeId, service.port());
                    params << Param(qCellsThingMacAddressParamTypeId, qcellsWallbox.macAddress());
                    params << Param(qCellsThingModbusIdParamTypeId, 1);
                    descriptor.setParams(params);
                    info->addThingDescriptor(descriptor);
                }
            }
            discoveryReply->deleteLater();
            info->finish(Thing::ThingErrorNoError);
        });
    }
}

void IntegrationPluginQCells::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcQcells()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == qCellsThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress =
                MacAddress(thing->paramValue(qCellsThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcQcells())
                    << "Failed to set up QCells wallbox because the MAC address is not valid:"
                    << thing->paramValue(qCellsThingMacAddressParamTypeId).toString()
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
    
        qCDebug(dcQcells()) << "Monitor reachable" << monitor->reachable()
                              << thing->paramValue(qCellsThingMacAddressParamTypeId).toString();
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

void IntegrationPluginQCells::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcQcells()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    uint port = thing->paramValue(qCellsThingPortParamTypeId).toUInt();
    quint16 modbusId = thing->paramValue(qCellsThingModbusIdParamTypeId).toUInt();
    QCellsModbusTcpConnection *connection = new QCellsModbusTcpConnection(
            monitor->networkDeviceInfo().address(), port, modbusId, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcQcells()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcQcells()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue(qCellsConnectedStateTypeId).toBool()) {
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
    connect(connection, &QCellsModbusTcpConnection::reachableChanged, thing,
            [this, connection, thing](bool reachable) {
                qCDebug(dcQcells()) << "Reachable state changed to" << reachable;
                if (reachable) {
                    // Connected true will be set after successfull init.
                    connection->initialize();
                } else {
                    thing->setStateValue(qCellsConnectedStateTypeId, false);
                    thing->setStateValue(qCellsCurrentPowerStateTypeId, 0);
                }
    });

    // Initialization has finished
    connect(connection, &QCellsModbusTcpConnection::initializationFinished, thing,
       [this, connection, thing](bool success) {
           thing->setStateValue(qCellsConnectedStateTypeId, success);

           if (success) {
                // Set basic info about device
               qCDebug(dcQcells()) << "QCells wallbox initialized.";
               quint16 firmware = connection->firmwareVersion();
               QString majorVersion = QString::number((firmware & 0xFF00) >> 8);
               QString minorVersion = QString::number(firmware & 0xFF);
               thing->setStateValue(qCellsFirmwareVersionStateTypeId,
                                    majorVersion+"."+minorVersion);
               thing->setStateValue(qCellsSerialNumberStateTypeId,
                                    connection->serialNumber());

               qCDebug(dcQcells()) << "Max Supported Power is" << connection->maxSupportedPower() << "kW";
               if (connection->maxSupportedPower() == 11) {
                   qCDebug(dcQcells()) << "Changed max current to 16"; 
                   thing->setStateMaxValue(qCellsMaxChargingCurrentStateTypeId, 16);
               } else {
                   qCDebug(dcQcells()) << "Changed max current to 32"; 
                   thing->setStateMaxValue(qCellsMaxChargingCurrentStateTypeId, 32);
               }
           } else {
               qCDebug(dcQcells()) << "QCells wallbox initialization failed.";
               // Try to reconnect to device
               connection->reconnectDevice();
           }
    });

    // Car has been plugged in or unplugged
    connect(connection, &QCellsModbusTcpConnection::plugStatusChanged, thing, [thing](quint16 state) {
        if (state == 0)
        {
            // not connected
            qCDebug(dcQcells()) << "Plug not connected";
            thing->setStateValue(qCellsPluggedInStateTypeId, false);
        } else {
            // connected
            qCDebug(dcQcells()) << "Plug connected";
            thing->setStateValue(qCellsPluggedInStateTypeId, true);
        }
    });

    // Set state for phase current A
    connect(connection, &QCellsModbusTcpConnection::currentPhaseAChanged, thing, [thing](float current) {
        qCDebug(dcQcells()) << "Current Phase A changed to" << current << "A";
        thing->setStateValue(qCellsCurrentPhaseAStateTypeId, current);
    });

    // Set state for phase current B
    connect(connection, &QCellsModbusTcpConnection::currentPhaseBChanged, thing, [thing](float current) {
        qCDebug(dcQcells()) << "Current Phase B changed to" << current << "A";
        thing->setStateValue(qCellsCurrentPhaseBStateTypeId, current);
    });

    // Set state for phase current C
    connect(connection, &QCellsModbusTcpConnection::currentPhaseCChanged, thing, [thing](float current) {
        qCDebug(dcQcells()) << "Current Phase C changed to" << current << "A";
        thing->setStateValue(qCellsCurrentPhaseCStateTypeId, current);
    });

    // Set state for current power
    connect(connection, &QCellsModbusTcpConnection::currentPowerChanged, thing, [thing](float power) {
        qCDebug(dcQcells()) << "Current Power changed to" << power << "kW";
        thing->setStateValue(qCellsCurrentPowerStateTypeId, power*1000);
    });

    // IMPORTANT: as of August 2024, the registers for sessionEnergy and totalConsumedEnergy
    // are mixed up in the data sheet
    // Set state for session energy
    connect(connection, &QCellsModbusTcpConnection::sessionEnergyConsumedChanged, thing, [thing](float energy) {
        qCDebug(dcQcells()) << "Session energy changed to" << energy << "kWh";
        thing->setStateValue(qCellsSessionEnergyStateTypeId, energy);
    });

    // Set state for total consumed energy
    connect(connection, &QCellsModbusTcpConnection::totalEnergyChanged, thing, [thing](float energy) {
        qCDebug(dcQcells()) << "Total energy changed to" << energy << "kWh";
        thing->setStateValue(qCellsTotalEnergyConsumedStateTypeId, energy);
    });

    // Check if alarm info has changed
    connect(connection, &QCellsModbusTcpConnection::alarmInfoChanged, thing, [thing](quint16 code) {
        qCDebug(dcQcells()) << "Alarm info changed to" << code;
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
        qCDebug(dcQcells()) << "Alaram code:" << code << "Alarm info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(qCellsAlarmInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(qCellsAlarmInfoStateTypeId, "No Error");
        }
    });

    // Check if fault info has changed
    connect(connection, &QCellsModbusTcpConnection::faultInfoChanged, thing, [thing](quint32 code) {
        qCDebug(dcQcells()) << "Fault info changed to" << code;
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
        qCDebug(dcQcells()) << "Fault code:" << code << "Fault info:" << warningMessage;
        if (warningMessage != "")
        {
            thing->setStateValue(qCellsFaultInfoStateTypeId, warningMessage);
        } else {
            thing->setStateValue(qCellsFaultInfoStateTypeId, "No Error");
        }
    });

    // Check if work mode has changed
    connect(connection, &QCellsModbusTcpConnection::workModeChanged, [this, thing, connection](quint16 mode) {
        qCDebug(dcQcells()) << "Work mode changed to" << mode << ". Make sure it is in controlled mode.";
        qCDebug(dcQcells()) << "Masked work mode:" << mode << "Masked current: " << maxCurrent;
        if (mode != 0)
        {
            qCDebug(dcQcells()) << "Setting workmode" << mode;
            QModbusReply *reply = connection->setWorkMode(mode);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                   qCDebug(dcQcells()) << "Successfully send command to set mode to 'controlled mode'";
                }
            });
        }
    });

    // Check if deviceStatus has changed
    connect(connection, &QCellsModbusTcpConnection::deviceStatusChanged, [this, thing](quint16 state) {
        qCDebug(dcQcells()) << "Device status changed to" << state;

        switch (state) {
        case QCellsModbusTcpConnection::EVCStatusIdle:
            thing->setStateValue(qCellsStateStateTypeId, "Available");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        case QCellsModbusTcpConnection::EVCStatusConnected:
            thing->setStateValue(qCellsStateStateTypeId, "Connected");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        case QCellsModbusTcpConnection::EVCStatusStarting:
            thing->setStateValue(qCellsStateStateTypeId, "Starting");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        case QCellsModbusTcpConnection::EVCStatusCharging:
            thing->setStateValue(qCellsStateStateTypeId, "Charging");
            thing->setStateValue(qCellsChargingStateTypeId, true);
            break;
        case QCellsModbusTcpConnection::EVCStatusPausing:
            thing->setStateValue(qCellsStateStateTypeId, "Paused");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        case QCellsModbusTcpConnection::EVCStatusFinishing:
            thing->setStateValue(qCellsStateStateTypeId, "Finished");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        case QCellsModbusTcpConnection::EVCStatusFaulted:
            thing->setStateValue(qCellsStateStateTypeId, "Faulted");
            thing->setStateValue(qCellsChargingStateTypeId, false);
            break;
        }
    });

    connect(m_chargeLimitTimer, &QTimer::timeout, this, [this, thing, connection]() {
        qCDebug(dcQcells()) << "m_chargeLimitTimer timeout.";
        float currentInApp = thing->stateValue(qCellsMaxChargingCurrentStateTypeId).toFloat();
        int phaseCount = thing->stateValue(qCellsPhaseCountStateTypeId).toUInt();
        setMaxCurrent(connection, currentInApp, phaseCount);
    });

    // Check if update has finished
    connect(connection, &QCellsModbusTcpConnection::updateFinished, thing, [this, thing, connection]() {
        qCDebug(dcQcells()) << "Update finished";
        float currentPhaseA = thing->stateValue(qCellsCurrentPhaseAStateTypeId).toFloat();
        float currentPhaseB = thing->stateValue(qCellsCurrentPhaseBStateTypeId).toFloat();
        float currentPhaseC = thing->stateValue(qCellsCurrentPhaseCStateTypeId).toFloat();

        // Set state count
        int phaseCount = 0;
        if (currentPhaseA > 0)
            phaseCount++;
        if (currentPhaseB > 0)
            phaseCount++;
        if (currentPhaseC > 0)
            phaseCount++;
        thing->setStateValue(qCellsPhaseCountStateTypeId, qMax(phaseCount,1));

        // TODO: Test working
        // If state is Charging, but power is false -> stop charging - DONE
        // if state available, connected, starting and power is true -> start charging - DONE
        QString state = thing->stateValue(qCellsStateStateTypeId).toString();
        bool power = thing->stateValue(qCellsPowerStateTypeId).toBool();
        if ((state == "Charging") && (power == false)) 
        {
            toggleCharging(connection, false);
        }

        if ((power == true) && ((state == "Available") ||
                                (state == "Connected") ||
                                (state == "Starting")  ||
                                (state == "Paused")    ||
                                (state == "Finished")))
        {
            toggleCharging(connection, true);
        }
        
        if (state == "Charging") {
            if (!m_chargeLimitTimer->isActive()) {
                qCDebug(dcQcells()) << "State is charging; Starting m_chargeLimitTimer.";
                m_chargeLimitTimer->start();
            }
        } else {
            if (m_chargeLimitTimer->isActive()) {
                qCDebug(dcQcells()) << "State is not charging; Stopping m_chargeLimitTimer.";
                m_chargeLimitTimer->stop();
            }
        }

        // Make current of wallbox follow app
        if ((state == "Charging") && (power == true)) {
            qCDebug(dcQcells()) << "Charging, but current is not corret. Correcting";
            double meanCurrent = (currentPhaseA + currentPhaseB + currentPhaseC) / phaseCount;
            float currentInApp = thing->stateValue(qCellsMaxChargingCurrentStateTypeId).toFloat();
            if ((meanCurrent > currentInApp+2) || (meanCurrent < currentInApp-2)) {
                setMaxCurrent(connection, currentInApp, phaseCount);
            }
        }
    });
    
    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginQCells::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    qCDebug(dcQcells()) << "Post setup thing..";

    if (!m_chargeLimitTimer)
        m_chargeLimitTimer = new QTimer(this);
    m_chargeLimitTimer->setInterval(45*1000);

    if (!m_pluginTimer) {
        qCDebug(dcQcells()) << "Starting plugin timer..";
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(3);
        connect(m_pluginTimer, &PluginTimer::timeout, this, [this] {
            qCDebug(dcQcells()) << "Updating QCells EVC..";
            foreach (QCellsModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginQCells::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    if (thing->thingClassId() == qCellsThingClassId)
    {
        QCellsModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (info->action().actionTypeId() == qCellsPowerActionTypeId)
        {
            qCDebug(dcQcells()) << "Start / Stop charging";
            bool power = info->action().paramValue(qCellsPowerActionPowerParamTypeId).toBool();
            toggleCharging(connection, power);
            thing->setStateValue(qCellsPowerStateTypeId, power);
            info->finish(Thing::ThingErrorNoError);
        }

        if (info->action().actionTypeId() == qCellsMaxChargingCurrentActionTypeId) {
            float maxCurrent = info->action().paramValue(qCellsMaxChargingCurrentActionMaxChargingCurrentParamTypeId).toFloat();
            int phaseCount = thing->stateValue(qCellsPhaseCountStateTypeId).toUInt();
            thing->setStateValue(qCellsMaxChargingCurrentStateTypeId, maxCurrent);
            setMaxCurrent(connection, maxCurrent, phaseCount);
            info->finish(Thing::ThingErrorNoError);
        }
    }
}

void IntegrationPluginQCells::toggleCharging(QCellsModbusTcpConnection *connection, bool power)
{
    quint16 startCharging = 1;
    quint16 stopCharging = 2;
    QModbusReply *reply = connection->setChargingControl(power ? startCharging : stopCharging);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, power, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcQcells()) << "Successfully set charge control";
        } else {
            qCDebug(dcQcells()) << "Toggle charge was not sent successfully";
            // toggleCharging(connection, power);
        }
    });
}

void IntegrationPluginQCells::setMaxCurrent(QCellsModbusTcpConnection *connection, float maxCurrent, int phaseCount)
{
    float maxPower = connection->maxChargePower();
    if (maxCurrent < 7)
        maxCurrent = 7;
    qCDebug(dcQcells()) << "Setting maxChargeCurrent to" << maxCurrent;
    qCDebug(dcQcells()) << "Setting maxChargePower to" << maxPower;
    maxPower = (230 * phaseCount * maxCurrent) / 1000;
    qCDebug(dcQcells()) << "Calculated power is" << maxPower;
    maxCurrent = maxPower;
    QModbusReply *reply = connection->setMaxChargeCurrent(maxCurrent);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, this, [this, connection, maxCurrent, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
           qCDebug(dcQcells()) << "Successfully set maximum charging current";
        } else {
            qCDebug(dcQcells()) << "Setting max current was not successfull";
            // setMaxCurrent(connection, maxCurrent);
        }
    });
}

void IntegrationPluginQCells::thingRemoved(Thing *thing)
{
    qCDebug(dcQcells()) << "Thing removed" << thing->name();

    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        QCellsModbusTcpConnection *connection = m_tcpConnections.take(thing);
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
