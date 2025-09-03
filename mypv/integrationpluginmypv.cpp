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

#include "plugininfo.h"
#include "integrationpluginmypv.h"

#include <network/networkdevicediscovery.h>

#include <QUdpSocket>
#include <QHostAddress>
#include <QEventLoop>
#include <QNetworkDatagram>

QString statusString(quint16 status, ThingClassId thingClassId)
{
    auto statusStr = QString{};
    if (thingClassId == acElwa2ThingClassId) {
        switch (status) {
            case 1:
                statusStr = "No Control";
                break;
            case 2:
                statusStr = "Heat";
                break;
            case 3:
                statusStr = "Standby";
                break;
            case 4:
                statusStr = "Boost heat";
                break;
            case 5:
                statusStr = "Heat finished";
                break;
            case 20:
                statusStr = "Legionella-Boost active";
                break;
            case 21:
                statusStr = "Device disabled (devmode = 0)";
                break;
            case 22:
                statusStr = "Device blocked";
                break;
            case 201:
                statusStr = "STL triggered";
                break;
            case 202:
                statusStr = "Powerstage Overtemp";
                break;
            case 203:
                statusStr = "Powerstage PCB temp probe fault";
                break;
            case 204:
                statusStr = "Hardware fault";
                break;
            case 205:
                statusStr = "ELWA Temp Sensor fault";
                break;
            case 209:
                statusStr = "Mainboard Error";
                break;
            default:
                statusStr = "Unknown";
                break;
        }
    } else if (thingClassId == acThor9sThingClassId ||
               thingClassId == acThorThingClassId) {
        if (status == 0) {
            statusStr = "Off";
        } else if (status >= 1 && status <= 8) {
            statusStr = "Device Startup";
        } else if (status == 9) {
            statusStr = "Operation";
        } else if (status >= 200) {
            statusStr = "Error";
        } else {
            statusStr = "Unknown";
        }
    } else {
        statusStr = "Unknown";
    }
    statusStr += " (" + QString::number(status) + ")";
    return statusStr;
}

int powerTimerInterval(quint16 powerTimeoutSeconds)
{
    return powerTimeoutSeconds < 5 ? 5000 : (powerTimeoutSeconds - 3) * 1000;
}

IntegrationPluginMyPv::IntegrationPluginMyPv()
{
}


void IntegrationPluginMyPv::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() != acElwa2ThingClassId &&
            info->thingClassId() != acThor9sThingClassId &&
            info->thingClassId() != acThorThingClassId) {
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class id: ") + info->thingClassId().toString());
        return;
    }

    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcMypv()) << "The network discovery is not available on this platform.";
        info->finish(Thing::ThingErrorUnsupportedFeature,
                     QT_TR_NOOP("The network device discovery is not available."));
        return;
    }

    QUdpSocket *searchSocket = new QUdpSocket{ this };
    // Make the socket stick to IPv4.
    const auto bindSuccess = searchSocket->bind(QHostAddress::AnyIPv4, 16124);
    qCDebug(dcMypv()) << "Search socket bind success:" << bindSuccess;

    QByteArray discoveryDatagram;
    if (info->thingClassId() == acElwa2ThingClassId) {
        discoveryDatagram.append(QByteArray::fromHex("a4d93f16"));
        discoveryDatagram.append(QString{"AC ELWA 2"}.toUtf8());
        discoveryDatagram.append(32 - discoveryDatagram.size(), '\0');
    } else if (info->thingClassId() == acThorThingClassId) {
        discoveryDatagram.append(QByteArray::fromHex("cb7a4e84"));
        discoveryDatagram.append(QString{"AC-THOR"}.toUtf8());
        discoveryDatagram.append(32 - discoveryDatagram.size(), '\0');
    } else if (info->thingClassId() == acThor9sThingClassId) {
        discoveryDatagram.append(QByteArray::fromHex("84db4f4c"));
        discoveryDatagram.append(QString{"AC-THOR 9s"}.toUtf8());
        discoveryDatagram.append(32 - discoveryDatagram.size(), '\0');
    }

    auto receivedDatagrams = QList<QNetworkDatagram>{};
    connect(searchSocket, &QUdpSocket::readyRead, this, [searchSocket, &receivedDatagrams]()
    {
        while (searchSocket->hasPendingDatagrams()) {
            const auto datagram = searchSocket->receiveDatagram();
            qCDebug(dcMypv()) << "Received datagram:" << datagram.data();
            if (datagram.data().size() != 64) {
                qCDebug(dcMypv()) << "Datagram length != 64. Ignoring invalid datagram!";
                continue;
            }
            receivedDatagrams << datagram;
        }
    });

    qCDebug(dcMypv()) << "Sending discovery datagram:" << discoveryDatagram << "length: " << discoveryDatagram.length();
    qint64 len = searchSocket->writeDatagram(discoveryDatagram, QHostAddress("255.255.255.255"), 16124);
    if (len != discoveryDatagram.length()) {
        searchSocket->deleteLater();
        info->finish(Thing::ThingErrorHardwareNotAvailable , tr("Error starting device discovery"));
        return;
    }

    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    // Wait for discovery to finish to get MAC address.
    if (hardwareManager()->networkDeviceDiscovery()->running()) {
        QEventLoop loop;
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    } else {
        // Wait some time to get UDP answers when discovery is not running.
        QEventLoop loop;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    QList<ThingDescriptor> descriptorList;
    foreach (const QNetworkDatagram &datagram, receivedDatagrams) {
        const auto rawData = datagram.data();
        const auto deviceId = rawData.mid(2, 2);
        auto deviceClassName = QString{};
        auto deviceClassId = ThingClassId{};
        if (deviceId == QByteArray::fromHex("3f16")) {
            deviceClassName = thingClass(acElwa2ThingClassId).displayName();
            deviceClassId = acElwa2ThingClassId;
        } else if (deviceId == QByteArray::fromHex("4e84")) {
            deviceClassName = thingClass(acThorThingClassId).displayName();
            deviceClassId = acThorThingClassId;
        } else if (deviceId == QByteArray::fromHex("4f4c")) {
            deviceClassName = thingClass(acThor9sThingClassId).displayName();
            deviceClassId = acThor9sThingClassId;
        } else {
            deviceClassName = QString{};
        }
        qCDebug(dcMypv())
                << "Found Device:"
                << (deviceClassName.isEmpty() ? QString{ "Unknown" } : deviceClassName);

        if (deviceClassName.isEmpty()) {
            qCDebug(dcMypv()) << "Ignoring unknown device!";
            continue;
        }
        if (deviceClassId != info->thingClassId()) {
            qCDebug(dcMypv())
                    << "Ignoring device which we did not look for:"
                    << deviceClassName;
            continue;
        }

        const auto senderAddress = datagram.senderAddress();
        ThingDescriptor thingDescriptor(info->thingClassId(), deviceClassName, senderAddress.toString());
        QByteArray serialNumber = rawData.mid(8, 16);

        foreach (Thing *existingThing, myThings()) {
            if (serialNumber == existingThing->paramValue("serialNumber").toString()) {
                qCDebug(dcMypv()) << "Rediscovered device " << existingThing->name();
                thingDescriptor.setThingId(existingThing->id());
                break;
            }
        }

        const auto networkDeviceInfo = discoveryReply->networkDeviceInfos().get(senderAddress);
        const auto macAddress = networkDeviceInfo.macAddress();
        if (macAddress.isNull()) {
            info->finish(Thing::ThingErrorInvalidParameter,
                         QT_TR_NOOP("The device was found, but the MAC address is invalid. Try searching again."));
        }
        ParamList params;
        if (info->thingClassId() == acElwa2ThingClassId) {
            params << Param(acElwa2ThingIpAddressParamTypeId, senderAddress.toString());
            params << Param(acElwa2ThingSerialNumberParamTypeId, serialNumber);
            params << Param(acElwa2ThingMacAddressParamTypeId, macAddress);
        } else if (info->thingClassId() == acThorThingClassId) {
            params << Param(acThorThingIpAddressParamTypeId, senderAddress.toString());
            params << Param(acThorThingSerialNumberParamTypeId, serialNumber);
            params << Param(acThorThingMacAddressParamTypeId, macAddress);
        } else if (info->thingClassId() == acThor9sThingClassId) {
            params << Param(acThor9sThingIpAddressParamTypeId, senderAddress.toString());
            params << Param(acThor9sThingSerialNumberParamTypeId, serialNumber);
            params << Param(acThor9sThingMacAddressParamTypeId, macAddress);
        }
        thingDescriptor.setParams(params);
        descriptorList << thingDescriptor;
    }
    info->addThingDescriptors(descriptorList);
    searchSocket->deleteLater();
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginMyPv::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    Q_UNUSED(thing)

    if (thing->thingClassId() == acElwa2ThingClassId ||
        thing->thingClassId() == acThor9sThingClassId ||
        thing->thingClassId() == acThorThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue("macAddress").toString());
        if (macAddress.isNull()) {
            qCWarning(dcMypv())
                    << "Failed to set up MyPV Heating Rod because the MAC address is not valid:"
                    << thing->paramValue("macAddress").toString()
                    << macAddress.toString();
            info->finish(Thing::ThingErrorInvalidParameter,
                         QT_TR_NOOP("The MAC address is not valid. Please reconfigure the device "
                                    "to fix this."));
            return;
        }

        // Create a monitor so we always get the correct IP in the network and see if the device is
        // reachable without polling on our own. In this call, nymea is checking a list for known
        // mac addresses and associated ip addresses
        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        // If the mac address is not known, nymea is starting a internal network discovery.
        // 'monitor' is returned while the discovery is still running -> monitor does not include ip
        // address and is set to not reachable
        m_monitors.insert(thing, monitor);

        qCDebug(dcMypv()) << "Monitor reachable" << monitor->reachable() << thing->paramValue("macAddress").toString();
        setupTcpConnection(info);
        info->finish(Thing::ThingErrorNoError);
    } else {
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class id: ") + thing->thingClassId().toString());
    }
}

void IntegrationPluginMyPv::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcMypv()) << "Setting up TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    MyPvModbusTcpConnection *connection =
            new MyPvModbusTcpConnection(monitor->networkDeviceInfo().address(),
                                        502,
                                        1,
                                        this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [this, thing]() {
        qCDebug(dcMypv()) << "Cleaning up because the setup has been aborted.";
        cleanUpThing(thing);
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [thing, connection, monitor](bool reachable) {
        qCDebug(dcMypv()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue("connected").toBool()) {
            connection->modbusTcpMaster()->setHostAddress(monitor->networkDeviceInfo().address());
            connection->reconnectDevice();
        } else {
            // Note: We disable autoreconnect explicitly and we will
            // connect the device once the monitor says it is reachable again
            connection->disconnectDevice();
        }
    });

    // Initialize if device is reachable again, otherwise set connected state to false
    // and power to 0
    connect(connection, &MyPvModbusTcpConnection::reachableChanged, thing, [connection, thing](bool reachable) {
        qCDebug(dcMypv()) << "Reachable state changed to" << reachable;
        if (reachable) {
            // Connected true will be set after successfull init.
            connection->initialize();
        } else {
            thing->setStateValue("connected", false);
        }
    });

    // Check if initilization works correctly
    connect(connection, &MyPvModbusTcpConnection::initializationFinished, thing,
            [this, connection, thing] (bool success) {
        thing->setStateValue("connected", success);
        if (success) {
            qCDebug(dcMypv()) << "my-PV Heating Rod intialized.";
            qCDebug(dcMypv()) << "Device type:" << thing->thingClass().displayName();
            const auto controllerFirmwareVersion =
                    QString{ "e%1%2" }
                    .arg(QString::number(connection->controllerFirmwareMainVersion()))
                    .arg(QString::number(connection->controllerFirmwareSubVersion()));
            thing->setStateValue("controllerFirmware", controllerFirmwareVersion);
            const auto powerstageFirmwareVersion = QString{ "ep%1" }
                    .arg(QString::number(connection->powerstageFirmwareVersion()));
            thing->setStateValue("powerstageFirmware", powerstageFirmwareVersion);
            configureConnection(connection);
        } else {
            qCDebug(dcMypv()) << "my-PV Heating Rod initialization failed.";
            connection->reconnectDevice();
        }
    });

    connect(connection, &MyPvModbusTcpConnection::waterTemperatureChanged, thing,
            [thing](float waterTemperature) {
        thing->setStateValue("waterTemperature", waterTemperature);
    });
    connect(connection, &MyPvModbusTcpConnection::targetWaterTemperatureChanged, thing,
            [thing](float targetWaterTemperature) {
        thing->setStateValue("targetWaterTemperature", targetWaterTemperature);
    });
    connect(connection, &MyPvModbusTcpConnection::statusChanged, thing,
            [thing](quint16 status) {
        const auto statusStr = statusString(status, thing->thingClassId());
        thing->setStateValue("status", statusStr);
    });
    connect(connection, &MyPvModbusTcpConnection::powerTimeoutChanged, thing,
            [this, thing](quint16 powerTimeout) {
        const auto timer = m_controlTimer.value(thing);
        timer->setInterval(powerTimerInterval(powerTimeout));
    });
    connect(connection, &MyPvModbusTcpConnection::minWaterTemperatureChanged, thing,
            [thing](float minWaterTemperature) {
        thing->setStateValue("minWaterTemperature", minWaterTemperature);
    });
    connect(connection, &MyPvModbusTcpConnection::controlTypeChanged, thing,
            [](quint16 controlType) {
        qCInfo(dcMypv()) << "Control Type changed to" << controlType;
        if (controlType != 2) {
            qCWarning(dcMypv())
                    << "Control type is not \"Modbus TCP\". Thing may not work correctly!";
        }
    });
    connect(connection, &MyPvModbusTcpConnection::currentPowerChanged, thing,
            [thing](quint16 currentPower) {
        thing->setStateValue("currentPower", currentPower);
    });
    connect(connection, &MyPvModbusTcpConnection::maxPowerChanged, thing,
            [thing](quint16 maxPower) {
        thing->setStateMaxValue("powerSetpointConsumer", maxPower);
    });
    connect(connection, &MyPvModbusTcpConnection::updateFinished, thing,
            [this, thing]() {
        const auto power = thing->stateValue("currentPower").toDouble();
        updatePowerConsumption(thing, power);
        m_lastUpdateTimestamp[thing] = QDateTime::currentDateTime();
    });


    const auto timer = new QTimer{ thing };
    timer->setSingleShot(true);
    m_controlTimer.insert(thing, timer);
    connect(timer, &QTimer::timeout, thing, [this, thing, timer, connection]() {
        writeHeatingPower(thing);
        // Restart timer to write heating power again before the last
        // written heating power value becomes invalid.
        timer->setInterval(powerTimerInterval(connection->powerTimeout()));
        timer->start();
    });

    if (monitor->reachable()) {
        connection->connectDevice();
    }
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginMyPv::configureConnection(MyPvModbusTcpConnection *connection)
{
    qCDebug(dcMypv()) << "Setting control type to \"Modbus TCP\"";
    const auto controlTypeModbusTCP = quint16{ 2 };
    const auto reply = connection->setControlType(controlTypeModbusTCP);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, reply, [reply]() {
        if (reply->error() != QModbusDevice::NoError) {
            qCWarning(dcMypv())
                    << "Setting control type failed:"
                    << reply->error()
                    << reply->errorString();
        }
    });

    QTimer::singleShot(1000, connection, [connection]() {
        qCDebug(dcMypv()) << "Setting power timeout to 10s";
        const auto powerTimeoutSeconds = quint16{ 10 };
        const auto reply = connection->setPowerTimeout(powerTimeoutSeconds);
        connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
        connect(reply, &QModbusReply::finished, reply, [reply]() {
            if (reply->error() != QModbusDevice::NoError) {
                qCWarning(dcMypv())
                        << "Setting power timeout failed:"
                        << reply->error()
                        << reply->errorString();
            }
        });
    });
}

void IntegrationPluginMyPv::writeHeatingPower(Thing *thing)
{
    auto connection = m_tcpConnections.value(thing);
    const auto heatingPower = static_cast<quint16>(thing->stateValue("powerSetpointConsumer").toDouble());
    const auto reply = connection->setCurrentPower(heatingPower);
    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
    connect(reply, &QModbusReply::finished, reply, [reply]() {
        if (reply->error() != QModbusDevice::NoError) {
            qCWarning(dcMypv())
                    << "Setting heating power failed:"
                    << reply->error()
                    << reply->errorString();
        }
    });
}

void IntegrationPluginMyPv::updatePowerConsumption(Thing *thing, double power)
{
    const auto lastUpdateTimepoint = m_lastUpdateTimestamp.value(thing, QDateTime{});
    if (!lastUpdateTimepoint.isValid()) { return; }
    const auto duration = QDateTime::currentMSecsSinceEpoch() - lastUpdateTimepoint.toMSecsSinceEpoch();
    const auto powerkWh = power / 1000.;
    const auto durationHours = duration / (1000. * 60. * 60.);
    const auto energyDiff = powerkWh * durationHours;
    const auto oldEnergy = thing->stateValue("totalEnergyConsumed").toDouble();
    const auto newEnergy = oldEnergy + energyDiff;
    thing->setStateValue("totalEnergyConsumed", newEnergy);
}

void IntegrationPluginMyPv::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    // Start plugin timer
    if (!m_refreshTimer) {
        qCDebug(dcMypv()) << "Starting plugin timer";
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach(MyPvModbusTcpConnection *connection, m_tcpConnections) {
                connection->update();
            }
        });
    }
}

void IntegrationPluginMyPv::thingRemoved(Thing *thing)
{
    cleanUpThing(thing);

    if (myThings().isEmpty()) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

void IntegrationPluginMyPv::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();

    if (thing->thingClassId() != acElwa2ThingClassId &&
        thing->thingClassId() != acThor9sThingClassId &&
        thing->thingClassId() != acThorThingClassId) {
        qCWarning(dcMypv())
                << "Unhandled thing class:"
                << thing->thingClass().displayName()
                << thing->thingClassId();
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class"));
        return;
    }
    if (!m_tcpConnections.contains(thing)) {
        qCWarning(dcMypv()) << "Can not find TCP connection for thing" << thing;
        info->finish(Thing::ThingErrorHardwareFailure,
                     QT_TR_NOOP("TCP connection not available"));
        return;
    }

    auto heatingPower = 0.;
    if (action.actionTypeId() == acElwa2PowerSetpointConsumerActionTypeId) {
        heatingPower = action.paramValue(acElwa2PowerSetpointConsumerActionPowerSetpointConsumerParamTypeId).toDouble();
    } else if (action.actionTypeId() == acThorPowerSetpointConsumerActionTypeId) {
        heatingPower = action.paramValue(acThorPowerSetpointConsumerActionPowerSetpointConsumerParamTypeId).toDouble();
    } else if (action.actionTypeId() == acThor9sPowerSetpointConsumerActionTypeId) {
        heatingPower = action.paramValue(acThor9sPowerSetpointConsumerActionPowerSetpointConsumerParamTypeId).toDouble();
    }

    auto enableHeating = false;
    if (action.actionTypeId() == acElwa2ActivateControlConsumerActionTypeId) {
        enableHeating = action.paramValue(acElwa2ActivateControlConsumerActionActivateControlConsumerParamTypeId).toBool();
    } else if (action.actionTypeId() == acThorActivateControlConsumerActionTypeId) {
        enableHeating = action.paramValue(acThorActivateControlConsumerActionActivateControlConsumerParamTypeId).toBool();
    } else if (action.actionTypeId() == acThor9sActivateControlConsumerActionTypeId) {
        enableHeating = action.paramValue(acThor9sActivateControlConsumerActionActivateControlConsumerParamTypeId).toBool();
    }

    const auto connection = m_tcpConnections.value(thing);
    if (action.actionTypeId() == acElwa2PowerSetpointConsumerActionTypeId ||
            action.actionTypeId() == acThorPowerSetpointConsumerActionTypeId ||
            action.actionTypeId() == acThor9sPowerSetpointConsumerActionTypeId) {
        thing->setStateValue("powerSetpointConsumer", heatingPower);
        const auto timer = m_controlTimer.value(thing);
        if (timer->isActive()) {
            timer->stop();
            writeHeatingPower(thing);
            timer->setInterval(powerTimerInterval(connection->powerTimeout()));
            timer->start();
        }
        info->finish(Thing::ThingErrorNoError);
    } else if (action.actionTypeId() == acElwa2ActivateControlConsumerActionTypeId ||
               action.actionTypeId() == acThorActivateControlConsumerActionTypeId ||
               action.actionTypeId() == acThor9sActivateControlConsumerActionTypeId) {
        thing->setStateValue("activateControlConsumer", enableHeating);
        const auto timer = m_controlTimer.value(thing);
        if (enableHeating) {
            writeHeatingPower(thing);
            timer->setInterval(powerTimerInterval(connection->powerTimeout()));
            timer->start();
        } else {
            timer->stop();
        }
        info->finish(Thing::ThingErrorNoError);
    } else {
        qCWarning(dcMypv()) << "Unhandled action type id:" << action.actionTypeId().toString();
        info->finish(Thing::ThingErrorActionTypeNotFound,
                     QT_TR_NOOP("Action type not found"));
    }
}

void IntegrationPluginMyPv::cleanUpThing(Thing *thing)
{
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        MyPvModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (m_lastUpdateTimestamp.contains(thing)) {
        m_lastUpdateTimestamp.remove(thing);
    }

    if (m_controlTimer.contains(thing)) {
        const auto timer = m_controlTimer.take(thing);
        if (timer->isActive()) {
            timer->stop();
        }
        timer->deleteLater();
    }
}
