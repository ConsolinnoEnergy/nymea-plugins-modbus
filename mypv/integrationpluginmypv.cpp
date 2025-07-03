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

QString boostModeString(quint16 boostMode,
                        quint16 operatingMode,
                        ThingClassId thingClassId)
{
    auto boostModeStr = QString{};
    if (thingClassId == acElwa2ThingClassId) {
        if (boostMode == 0) {
            boostModeStr = "Off";
        } else if (boostMode == 1) {
            if (operatingMode == 0) {
                boostModeStr = "On (ELWA)";
            } else if (operatingMode == 3) {
                boostModeStr = "On (ELWA + AUX)";
            } else {
                boostModeStr = "Unknown";
            }
        } else if (boostMode == 4) {
            boostModeStr = "SELV";
        } else if (boostMode == 5) {
            boostModeStr = "AUX";
        } else {
            boostModeStr = "Unknown";
        }
    } else if (thingClassId == acThor9sThingClassId ||
               thingClassId == acThorThingClassId) {
        if (boostMode == 0) {
            boostModeStr = "Off";
        } else if (boostMode == 1) {
            boostModeStr = "On";
        } else if (boostMode == 3) {
            boostModeStr = "Relay Boost On";
        } else {
            boostModeStr = "Unknown";
        }
    } else {
        boostModeStr = "Unknown";
    }
    boostModeStr += " (" + QString::number(boostMode) + ")";
    return boostModeStr;
}

QString controlTypeString(quint16 controlType, ThingClassId thingClassId)
{
    if (thingClassId != acElwa2ThingClassId &&
        thingClassId != acThor9sThingClassId &&
        thingClassId != acThorThingClassId) {
        return "Unknown";
    }

    const auto isElwa = (thingClassId == acElwa2ThingClassId);
    const auto isThor = !isElwa;
    auto controlTypeStr = QString{};
    switch (controlType) {
        case 0:
            controlTypeStr = isElwa ? "Auto Detect" : "Unknown";
            break;
        case 1:
            controlTypeStr = "HTTP";
            break;
        case 2:
            controlTypeStr = "Modbus TCP";
            break;
        case 3:
            controlTypeStr = "Fronius Auto";
            break;
        case 4:
            controlTypeStr = "Fronius Manual";
            break;
        case 5:
            controlTypeStr = "SMA Home Manager";
            break;
        case 6:
            controlTypeStr = "Steca Auto";
            break;
        case 7:
            controlTypeStr = "Varta Auto";
            break;
        case 8:
            controlTypeStr = "Varta Manual";
            break;
        case 9:
            controlTypeStr = isThor ? "my-PV Power Meter Auto" : "Unknown";
            break;
        case 10:
            controlTypeStr = isThor ? "my-PV Power Meter Manual" : "RCT Power Manual";
            break;
        case 11:
            controlTypeStr = isThor ? "my-PV Power Meter Direct" : "Unknown";
            break;
        case 12:
            controlTypeStr = isElwa ? "my-PV Meter Auto" : "Unknown";
            break;
        case 13:
            controlTypeStr = isElwa ? "my-PV Meter Manual" : "Unknown";
            break;
        case 14:
            controlTypeStr = isElwa ? "my-PV Meter Direct" : "RCT Power Manual";
            break;
        case 15:
            controlTypeStr = isElwa ? "SMA Direct meter communication Auto" : "Unknown";
            break;
        case 16:
            controlTypeStr = isElwa ? "SMA Direct meter communication Manual" : "Unknown";
            break;
        case 17:
            controlTypeStr = isThor ? "SMA Direct meter communication Auto" : "Unknown";
            break;
        case 18:
            controlTypeStr = isThor ? "SMA Direct meter communication Manual" : "Unknown";
            break;
        case 19:
            controlTypeStr = "Digital Meter P1";
            break;
        case 20:
            controlTypeStr = "Frequency";
            break;
        case 100:
            controlTypeStr = "Fronius Sunspec Manual";
            break;
        case 101:
            controlTypeStr = isThor ? "KACO TL1 + TL3 Manual" : "Unknown";
            break;
        case 102:
            controlTypeStr = "Kostal PIKO IQ Plenticore plus Manual";
            break;
        case 103:
            controlTypeStr = "Kostal Smart Energy Meter Manual";
            break;
        case 104:
            controlTypeStr = "MEC electronics Manual";
            break;
        case 105:
            controlTypeStr = "SolarEdge Manual";
            break;
        case 106:
            controlTypeStr = "Victron Energy 1ph Manual";
            break;
        case 107:
            controlTypeStr = "Victron Energy 3ph Manual";
            break;
        case 108:
            controlTypeStr = "Huawei (Modbus TCP) Manual";
            break;
        case 109:
            controlTypeStr = "Carlo Gavazzi EM24 Manual";
            break;
        case 111:
            controlTypeStr = "Sungrow Manual";
            break;
        case 112:
            controlTypeStr = "Fronius Gen24 Manual";
            break;
        case 113:
            controlTypeStr = isThor ? "GoodWe Manual" : "Unknown";
            break;
        case 200:
            controlTypeStr = "Huawei (Modbus RTU)";
            break;
        case 201:
            controlTypeStr = "Growatt (Modbus RTU)";
            break;
        case 202:
            controlTypeStr = "Solax (Modbus RTU)";
            break;
        case 203:
            controlTypeStr = "Qcells (Modbus RTU)";
            break;
        case 204:
            controlTypeStr = "IME Conto D4 Modbus MID (Modbus RTU)";
            break;
        default:
            controlTypeStr = "Unknown";
            break;
    }
    controlTypeStr += " (" + QString::number(controlType) + ")";
    return controlTypeStr;
}

int heatingPowerInterval(Thing *thing)
{
    return thing->stateValue("powerTimeout").toUInt() - 3000;
}

IntegrationPluginMyPv::IntegrationPluginMyPv()
{
}


void IntegrationPluginMyPv::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == acElwa2ThingClassId ||
        info->thingClassId() == acThor9sThingClassId ||
        info->thingClassId() == acThorThingClassId) {

        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcMypv()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature,
                         QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        QUdpSocket *searchSocket = new QUdpSocket(this);

        // Note: This will fail, and it's not a problem, but it is required to force the socket to stick to IPv4...
        const auto bindSuccess = searchSocket->bind(QHostAddress::AnyIPv4, 16124);
        qCDebug(dcMypv()) << "Search socket bind success:" << bindSuccess;

        QByteArray discoveryDatagram;
        // TODO: These might need to be changed (at least for AC Thor, discovery worked for ELWA 2)
        // AC ELWA 2    - a4d93f16
        // AC THOR      - cb7a4e84
        // AC THOR 9s   - 84db4f4c
        // POWER METER  - 401e4e8e
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
            auto deviceType = QString{};
            if (deviceId == QByteArray::fromHex("3f16")) {
                deviceType = thingClass(acElwa2ThingClassId).displayName();
            } else if (deviceId == QByteArray::fromHex("4e84")) {
                deviceType = thingClass(acThorThingClassId).displayName();
            } else if (deviceId == QByteArray::fromHex("4f4c")) {
                deviceType = thingClass(acThor9sThingClassId).displayName();
            } else {
                deviceType = QString{};
            }
            qCDebug(dcMypv())
                    << "Found Device:"
                    << (deviceType.isEmpty() ? QString{ "Unknown" } : deviceType);

            if (deviceType.isEmpty()) {
                qCDebug(dcMypv()) << "Ignoring unknown device!";
                continue;
            }

            const auto senderAddress = datagram.senderAddress();
            ThingDescriptor thingDescriptor(info->thingClassId(), deviceType, senderAddress.toString());
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
    } else {
        info->finish(Thing::ThingErrorThingClassNotFound,
                     QT_TR_NOOP("Unknown thing class id: ") + info->thingClassId().toString());
    }
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
        m_setupTcpConnectionRunning = false;
        if (monitor->reachable()) {
            setupTcpConnection(info);
        } else {
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [this, info]() {
                    if (!m_setupTcpConnectionRunning) {
                        m_setupTcpConnectionRunning = true;
                        setupTcpConnection(info);
                    }
            });
        }
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
    MyPvModbusTcpConnection *connection = new MyPvModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [this, thing]() {
        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcMypv()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }

        if (m_tcpConnections.contains(thing)) {
            MyPvModbusTcpConnection *connection = m_tcpConnections.take(thing);
            connection->disconnectDevice();
            connection->deleteLater();
        }
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
        thing->setStateValue("powerTimeout", powerTimeout);
        const auto timer = m_controlTimer.value(thing);
        timer->setInterval(heatingPowerInterval(thing));
    });
    connect(connection, &MyPvModbusTcpConnection::boostModeChanged, thing,
            [thing, connection](quint16 boostMode) {
        const auto boostModeStr = boostModeString(boostMode,
                                                  connection->operationMode(),
                                                  thing->thingClassId());
        thing->setStateValue("boostMode", boostModeStr);
    });
    connect(connection, &MyPvModbusTcpConnection::minWaterTemperatureChanged, thing,
            [thing](float minWaterTemperature) {
        thing->setStateValue("minWaterTemperature", minWaterTemperature);
    });
    connect(connection, &MyPvModbusTcpConnection::meterPowerChanged, thing,
            [thing](qint16 meterPower) {
        thing->setStateValue("meterPower", meterPower);
    });
    connect(connection, &MyPvModbusTcpConnection::controlTypeChanged, thing,
            [thing](quint16 controlType) {
        const auto controlTypeStr = controlTypeString(controlType,
                                                      thing->thingClassId());
        thing->setStateValue("controlType", controlTypeStr);
    });
    connect(connection, &MyPvModbusTcpConnection::maxPowerAbsChanged, thing,
            [thing](quint16 maxPowerAbs) {
        thing->setStateValue("maxPowerAbs", maxPowerAbs);
    });
    connect(connection, &MyPvModbusTcpConnection::devicePowerChanged, thing,
            [thing](quint16 devicePower) {
        thing->setStateValue("devicePower", devicePower);
    });
    connect(connection, &MyPvModbusTcpConnection::devicePowerSolarChanged, thing,
            [thing](quint16 devicePowerSolar) {
        thing->setStateValue("devicePowerSolar", devicePowerSolar);
    });
    connect(connection, &MyPvModbusTcpConnection::devicePowerGridChanged, thing,
            [thing](quint16 devicePowerGrid) {
        thing->setStateValue("devicePowerGrid", devicePowerGrid);
    });
    connect(connection, &MyPvModbusTcpConnection::currentPowerChanged, thing,
            [thing](quint16 currentPower) {
        thing->setStateValue("heatingPower", currentPower);
    });
    connect(connection, &MyPvModbusTcpConnection::maxPowerChanged, thing,
            [thing](quint16 maxPower) {
        thing->setStateValue("maxPower", maxPower);
        thing->setStateMaxValue("heatingPower", maxPower);
    });
    connect(connection, &MyPvModbusTcpConnection::externalTemperatureChanged, thing,
            [thing](float externalTemperature) {
        thing->setStateValue("externalTemperature", externalTemperature);
    });
    connect(connection, &MyPvModbusTcpConnection::operationModeChanged, thing,
            [thing](quint16 operationMode) {
        thing->setStateValue("operationMode", operationMode);
    });

    // #TODO remove when testing finished
    connect(connection, &MyPvModbusTcpConnection::updateFinished, connection, [connection]() {
        qCDebug(dcMypv()) << connection;
    });

    const auto timer = new QTimer{ thing };
    timer->setSingleShot(true);
    m_controlTimer.insert(thing, timer);
    connect(timer, &QTimer::timeout, thing, [this, thing, timer]() {
        writeHeatingPower(thing);
        // Restart timer to write heating power again before the last
        // written heating power value becomes invalid.
        timer->setInterval(heatingPowerInterval(thing));
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
    const auto heatingPower = m_setHeatingPower.value(thing, quint16{ 0 });
    qCDebug(dcMypv()) << "Setting heating power to" << heatingPower; // #TODO remove when testing finished
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
    if (m_monitors.contains(thing)) {
        hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
    }

    if (m_tcpConnections.contains(thing)) {
        MyPvModbusTcpConnection *connection = m_tcpConnections.take(thing);
        connection->disconnectDevice();
        connection->deleteLater();
    }

    if (m_setHeatingPower.contains(thing)) {
        m_setHeatingPower.remove(thing);
    }

    if (m_controlTimer.contains(thing)) {
        const auto timer = m_controlTimer.take(thing);
        if (timer->isActive()) {
            timer->stop();
        }
        timer->deleteLater();
    }

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
        return;
    }

    auto heatingPower = quint16{ 0 };
    if (action.actionTypeId() == acElwa2HeatingPowerActionTypeId) {
        heatingPower = action.paramValue(acElwa2HeatingPowerActionHeatingPowerParamTypeId).toUInt();
    } else if (action.actionTypeId() == acThorHeatingPowerActionTypeId) {
        heatingPower = action.paramValue(acThorHeatingPowerActionHeatingPowerParamTypeId).toUInt();
    } else if (action.actionTypeId() == acThor9sHeatingPowerActionTypeId) {
        heatingPower = action.paramValue(acThor9sHeatingPowerActionHeatingPowerParamTypeId).toUInt();
    }

    auto enableHeating = false;
    if (action.actionTypeId() == acElwa2ExternalControlActionTypeId) {
        enableHeating = action.paramValue(acElwa2ExternalControlActionExternalControlParamTypeId).toBool();
    } else if (action.actionTypeId() == acThorExternalControlActionTypeId) {
        enableHeating = action.paramValue(acThorExternalControlActionExternalControlParamTypeId).toBool();
    } else if (action.actionTypeId() == acThor9sExternalControlActionTypeId) {
        enableHeating = action.paramValue(acThor9sExternalControlActionExternalControlParamTypeId).toBool();
    }

    if (action.actionTypeId() == acElwa2HeatingPowerActionTypeId ||
            action.actionTypeId() == acThorHeatingPowerActionTypeId ||
            action.actionTypeId() == acThor9sHeatingPowerActionTypeId) {
        qCDebug(dcMypv()) << "New heating power to set:" << heatingPower;
        m_setHeatingPower[thing] = heatingPower;
        const auto timer = m_controlTimer.value(thing);
        if (timer->isActive()) {
            timer->stop();
            writeHeatingPower(thing);
            timer->setInterval(heatingPowerInterval(thing));
            timer->start();
        }
    } else if (action.actionTypeId() == acElwa2ExternalControlActionTypeId ||
               action.actionTypeId() == acThorExternalControlActionTypeId ||
               action.actionTypeId() == acThor9sExternalControlActionTypeId) {
        const auto timer = m_controlTimer.value(thing);
        if (enableHeating) {
            writeHeatingPower(thing);
            timer->setInterval(heatingPowerInterval(thing));
            timer->start();
        } else {
            timer->stop();
        }
    } else {
        qCWarning(dcMypv()) << "Unhandled action type id:" << action.actionTypeId().toString();
    }

    Q_UNUSED(info);
    // #TODO everything
    /*
    qCDebug(dcMypv()) << "Executing action for" << info->thing();
    Thing *thing = info->thing();
    Action action = info->action();
    
    StateTypeId heatingPowerActionId = thing->state("heatingPower").stateTypeId();
    StateTypeId externalControlActionId = thing->state("externalControl").stateTypeId();
    
    if (thing->thingClassId() == acElwa2ThingClassId ||
        thing->thingClassId() == acThor9sThingClassId ||
        thing->thingClassId() == acThorThingClassId) {
        MyPvModbusTcpConnection *connection = m_tcpConnections.value(thing);
        if (action.actionTypeId() == heatingPowerActionId) {
            // Set the heating power of the heating rod
            qCDebug(dcMypv()) << "Set heating power";
            // int heatingPower = action.param(elwaHeatingPowerActionHeatingPowerParamTypeId).value().toInt();
            int heatingPower = action.params()[0].value().toInt();
            // TODO: Setzen von der Leistung über setCurrentPower() oder setMaxPower()
            // Wenn maxPower: Muss dies nach deaktivieren der externen Controlle wieder auf max moeglichen Wert gesetzt werden?
            QModbusReply *reply = connection->setMaxPower(heatingPower);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply, heatingPower]() {
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcMypv()) << "Heating power set successfully";
                } else {
                    qCDebug(dcMypv()) << "Error setting heating power";
                }
            });
            thing->setStateValue(acElwa2HeatingPowerStateTypeId, heatingPower);
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == externalControlActionId) {
            // Manually start the heating rod
            qCDebug(dcMypv()) << "Manually start heating rod";
            // bool power = action.param(elwaExternalControlActionExternalControlParamTypeId).value().toBool();
            bool power = action.params()[0].value().toBool();

            // Save previously configured values; so we can restore standard regulation
            // when heating rod should not be controlled by HEMS
            if (power == true) {
                thing->setStateValue("oldBoostMode", connection->boostMode());
                thing->setStateValue("oldBoostActive", connection->manualStart());
            }

            // For ELWA 2, manual needs to be set to 2 to manually actviate boost mode
            // mode = 0 - off / pv surpslus heating / internal logic
            // mode = 1 - autoboost
            // mode = 2 - manual boost
            QModbusReply *reply = connection->setBoostMode(power ? 0 : thing->stateValue("oldBoostMode").toUInt());
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply, connection]() {
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcMypv()) << "Successfully startet heating rod";

                    QModbusReply *reply = connection->setMaxPower(m_devicePower[m_myDevice]);
                    connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                    connect(reply, &QModbusReply::finished, this, [this, reply]() {
                        if (reply->error() == QModbusDevice::NoError) {
                            qCDebug(dcMypv()) << "Heating power set successfully";
                        } else {
                            qCDebug(dcMypv()) << "Error setting heating power";
                        }
                    });

                } else {
                    qCDebug(dcMypv()) << "Error starting heating power";
                }
            });
            thing->setStateValue(acElwa2ExternalControlStateTypeId, power);
            info->finish(Thing::ThingErrorNoError);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
        }
    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }
    */
}
