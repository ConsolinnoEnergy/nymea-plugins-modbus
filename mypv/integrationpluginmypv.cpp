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

#include <QUdpSocket>
#include <QHostAddress>
#include <network/networkdevicediscovery.h>
#include <QEventLoop>

IntegrationPluginMyPv::IntegrationPluginMyPv()
{
}


void IntegrationPluginMyPv::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == elwaThingClassId ||
        info->thingClassId() == acThor9sThingClassId ||
        info->thingClassId() == acThorThingClassId) {

        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcMypv()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        QUdpSocket *searchSocket = new QUdpSocket(this);

        // Note: This will fail, and it's not a problem, but it is required to force the socket to stick to IPv4...
        searchSocket->bind(QHostAddress::AnyIPv4, 16124);

        QByteArray discoveryString;
        // TODO: These might need to be changed (at least for AC Thor, discovery worked for ELWA 2)
        // AC ELWA 2    - a4d93f16
        // AC THOR      - cb7a4e84
        // AC THOR 9s   - 84db4f4c
        discoveryString.resize(19);
        discoveryString.fill(0);
        discoveryString.insert(0, QByteArray::fromHex("86d93efc"));
        discoveryString.insert(4, "AC ELWA-E");

        qCDebug(dcMypv()) << "Send datagram:" << discoveryString << "length: " << discoveryString.length();
        qint64 len = searchSocket->writeDatagram(discoveryString, QHostAddress("255.255.255.255"), 16124);
        if (len != discoveryString.length()) {
            searchSocket->deleteLater();
            info->finish(Thing::ThingErrorHardwareNotAvailable , tr("Error starting device discovery"));
            return;
        }

        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();

        QTimer::singleShot(2000, this, [this, searchSocket, info, discoveryReply](){
            QList<ThingDescriptor> descriptorList;
            while(searchSocket->hasPendingDatagrams()) {
                char buffer[1024];
                QHostAddress senderAddress;

                int len = searchSocket->readDatagram(buffer, 1024, &senderAddress);
                QByteArray data = QByteArray::fromRawData(buffer, len);
                qCDebug(dcMypv()) << "Have datagram:" << data;
                if (data.length() < 64) {
                    continue;
                }

                //Device Id AC•THOR = 0x4e84
                //Device Id AC•THOR 9s = 0x4f4c
                //Device Id Power  = 0x4e8e
                //Device Id AC ELWA-E = 0x3efc
                //Device Id AC ELWA-2 = 0x3f16
                QString device;
                qCDebug(dcMypv()) << "device Id:" << data.mid(2, 2);
                if (data.mid(2, 2) == QByteArray::fromHex("3efc")) {
                    qCDebug(dcMypv()) << "Found Device: AC ElWA-E";
                    device = "AC ELWA-E";
                    m_myDevice = AC_ELWA_E;
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x3f16")) {
                    qCDebug(dcMypv()) << "Found Device: AC ELWA-2";
                    device = "AC ELWA-2";
                    m_myDevice = AC_ELWA_2;
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x4e8e")) {
                    qCDebug(dcMypv()) << "Found Device: Powermeter";
                    device = QT_TR_NOOP("my-PV Meter");
                    m_myDevice = POWER_METER;
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x4e84")) {
                    qCDebug(dcMypv()) << "Found Device: AC Thor";
                    device = "AC Thor";
                    m_myDevice = AC_THOR;
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x4f4c")) {
                    qCDebug(dcMypv()) << "Found Device: AC Thor 9s";
                    device = "AC Thor 9s";
                    m_myDevice = AC_THOR_9s;
                } else {
                    qCDebug(dcMypv()) << "Failed to parse discovery datagram from" << senderAddress << data;
                    device = "Invalid";
                    continue;
                }

                ThingDescriptor thingDescriptors(info->thingClassId(), device, senderAddress.toString());
                QByteArray serialNumber = data.mid(8, 16);

                foreach (Thing *existingThing, myThings()) {
                    if (serialNumber == existingThing->paramValue("serialNumber").toString()) {
                        qCDebug(dcMypv()) << "Rediscovered device " << existingThing->name();
                        thingDescriptors.setThingId(existingThing->id());
                        break;
                    }
                }

                if (hardwareManager()->networkDeviceDiscovery()->running()) {
                    QEventLoop loop;
                    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, &loop, &QEventLoop::quit);
                    loop.exec();
                }
                NetworkDeviceInfo heatingRod = discoveryReply->networkDeviceInfos().get(senderAddress);
                if (heatingRod.macAddress().isNull()) {
                    info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The device was found, but the MAC address is invalid. Try searching again."));
                }

                ParamList params;
                if (info->thingClassId() == elwaThingClassId) {
                    params << Param(elwaThingIpAddressParamTypeId, senderAddress.toString());
                    params << Param(elwaThingSerialNumberParamTypeId, serialNumber);
                    params << Param(elwaThingMacAddressParamTypeId, heatingRod.macAddress());
                } else if (info->thingClassId() == acThorThingClassId) {
                    params << Param(acThorThingIpAddressParamTypeId, senderAddress.toString());
                    params << Param(acThorThingSerialNumberParamTypeId, serialNumber);
                    params << Param(acThorThingMacAddressParamTypeId, heatingRod.macAddress());
                } else if (info->thingClassId() == acThor9sThingClassId) {
                    params << Param(acThor9sThingIpAddressParamTypeId, senderAddress.toString());
                    params << Param(acThor9sThingSerialNumberParamTypeId, serialNumber);
                    params << Param(acThor9sThingMacAddressParamTypeId, heatingRod.macAddress());
                }

                thingDescriptors.setParams(params);
                descriptorList << thingDescriptors;
            }
            info->addThingDescriptors(descriptorList);;
            searchSocket->deleteLater();
            info->finish(Thing::ThingErrorNoError);
        });
    } else {
        Q_ASSERT_X(false, "discoverThings", QString("Unhandled thingClassId: %1").arg(info->thingClassId().toString()).toUtf8());
    }
}

void IntegrationPluginMyPv::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    Q_UNUSED(thing)

    if (thing->thingClassId() == elwaThingClassId ||
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
                         QT_TR_NOOP("The MAC address is not vaild. Please reconfigure the device "
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
        Q_ASSERT_X(false, "setupThing", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }
}

void IntegrationPluginMyPv::setupTcpConnection(ThingSetupInfo *info)
{
    qCDebug(dcMypv()) << "Setup TCP connection.";
    Thing *thing = info->thing();
    NetworkDeviceMonitor *monitor = m_monitors.value(info->thing());
    MyPvModbusTcpConnection *connection = new MyPvModbusTcpConnection(monitor->networkDeviceInfo().address(), 502, 1, this);
    m_tcpConnections.insert(thing, connection);

    connect(info, &ThingSetupInfo::aborted, monitor, [=]() {
        // Is this needed? How can setup be aborted at this point?

        // Clean up in case the setup gets aborted.
        if (m_monitors.contains(thing)) {
            qCDebug(dcMypv()) << "Unregister monitor because the setup has been aborted.";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }
    });

    // Reconnect on monitor reachable changed
    connect(monitor, &NetworkDeviceMonitor::reachableChanged, thing, [=](bool reachable) {
        qCDebug(dcMypv()) << "Network device monitor reachable changed for" << thing->name()
                              << reachable;
        if (reachable && !thing->stateValue("connected").toBool()) {
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
    connect(connection, &MyPvModbusTcpConnection::reachableChanged, thing, [this, connection, thing](bool reachable) {
        qCDebug(dcMypv()) << "Reachable state changed to" << reachable;
        if (reachable) {
            // Connected true will be set after successfull init.
            connection->initialize();
        } else {
            thing->setStateValue("connected", false);
        }
    });

    // Check if initilization works correctly
    connect(connection, &MyPvModbusTcpConnection::initializationFinished, thing, [this, connection, thing] (bool success) {
        thing->setStateValue("connected", success);
        if (success) {
            qCDebug(dcMypv()) << "my-PV Heating Rod intialized.";
            qCDebug(dcMypv()) << "### Current device is" << m_myDevice;
            qCDebug(dcMypv()) << "### Max Power should be set to" << m_devicePower[m_myDevice];
            thing->setStateMaxValue("heatingPower", m_devicePower[m_myDevice]);
        } else {
            qCDebug(dcMypv()) << "my-PV Heating Rod initialization failed.";
            connection->reconnectDevice();
        }
    });

    // Read the current power consumed by the device
    connect(connection, &MyPvModbusTcpConnection::currentPowerChanged, thing, [thing](quint16 power) {
        qCDebug(dcMypv()) << "Current power changed" << power << "W";
        thing->setStateValue("currentPower", power);
    });

    // Read the currently measured water temperature
    connect(connection, &MyPvModbusTcpConnection::waterTemperatureChanged, thing, [thing](double temp) {
        qCDebug(dcMypv()) << "Actual water temperature changed" << temp << "°C";
        thing->setStateValue("temperature", temp);
    });

    // Read the set target water temperature
    connect(connection, &MyPvModbusTcpConnection::targetWaterTemperatureChanged, thing, [thing](double temp) {
        qCDebug(dcMypv()) << "Target water temperature changed" << temp << "°C";
        thing->setStateValue("targetWaterTemperature", temp);
    });

    connect(connection, &MyPvModbusTcpConnection::maxPowerChanged, thing, [thing](quint16 power) {
        qCDebug(dcMypv()) << "Max power changed to" << power;
        thing->setStateValue("maxPower", power);
    });

    connect(connection, &MyPvModbusTcpConnection::meterPowerChanged, thing, [thing](qint16 power) {
        qCDebug(dcMypv()) << "Meter power changed to" << power;
        thing->setStateValue("meterPower", power);
    });

    connect(connection, &MyPvModbusTcpConnection::immHeaterPowerChanged, thing, [thing](qint16 power) {
        qCDebug(dcMypv()) << "Immersion heater power changed to" << power;
        thing->setStateValue("immersionHeaterPower", power);
    });

    connect(connection, &MyPvModbusTcpConnection::auxRelayPowerChanged, thing, [thing](qint16 power) {
        qCDebug(dcMypv()) << "AUX relay power changed to" << power;
        thing->setStateValue("auxRelayPower", power);
    });

    connect(connection, &MyPvModbusTcpConnection::powerTimeoutChanged, thing, [thing](quint16 timeout) {
        qCDebug(dcMypv()) << "Power timeout changed to" << timeout;
        thing->setStateValue("powerTimeout", timeout);
    });

    // Read the current status of the heating rod
    /*
    connect(connection, &MyPvModbusTcpConnection::elwaStatusChanged, thing, [thing](quint16 state) {
        qCDebug(dcMypv()) << "State changed" << state;
        switch (state) {
        case MyPvModbusTcpConnection::ElwaStatusHeating:
            thing->setStateValue("status", QT_TR_NOOP("Heating"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusStandby:
            thing->setStateValue("status", QT_TR_NOOP("Standby"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusBoosted:
            thing->setStateValue("status", QT_TR_NOOP("Boosted"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusHeatFinished:
            thing->setStateValue("status", QT_TR_NOOP("Heating Finished"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusSetup:
            thing->setStateValue("status", QT_TR_NOOP("Setup"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusLageionellaBoost:
            thing->setStateValue("status", QT_TR_NOOP("Lageionella Boost"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusDeviceDisabled:
            thing->setStateValue("status", QT_TR_NOOP("Device Disabled"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusDeviceBlocked:
            thing->setStateValue("status", QT_TR_NOOP("Device Blocked"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempFuseBlown:
            thing->setStateValue("status", QT_TR_NOOP("Error Overtemp Fuse Blown"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempMeasured:
            thing->setStateValue("status", QT_TR_NOOP("Error Overtemp Measured"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempElectronics:
            thing->setStateValue("status", QT_TR_NOOP("Error Overtemp Electronics"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorHardwareFault:
            thing->setStateValue("status", QT_TR_NOOP("Error Hardware Fault"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorTempSensor:
            thing->setStateValue("status", QT_TR_NOOP("Error Temperatur Sensor"));
            break;
        default:
            thing->setStateValue("status", QT_TR_NOOP("Unknown"));
            break;
        }
    });
    */

    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginMyPv::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    // Start plugin timer
    if (!m_refreshTimer) {
        qCDebug(dcMypv()) << "Starting plugin timer";
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach(MyPvModbusTcpConnection *connection, m_tcpConnections) {
                qCDebug(dcMypv()) << "Updated my-PV Heating Rod";
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

    if (myThings().isEmpty()) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
}

void IntegrationPluginMyPv::executeAction(ThingActionInfo *info)
{
    qCDebug(dcMypv()) << "Executing action for" << info->thing();
    Thing *thing = info->thing();
    Action action = info->action();
    
    StateTypeId heatingPowerActionId = thing->state("heatingPower").stateTypeId();
    StateTypeId externalControlActionId = thing->state("externalControl").stateTypeId();
    
    if (thing->thingClassId() == elwaThingClassId ||
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
            QModbusReply *reply = connection->setCurrentPower(heatingPower);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply, heatingPower]() {
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcMypv()) << "Heating power set successfully";
                } else {
                    qCDebug(dcMypv()) << "Error setting heating power";
                }
            });
            thing->setStateValue(elwaHeatingPowerStateTypeId, heatingPower);
            info->finish(Thing::ThingErrorNoError);
        } else if (action.actionTypeId() == externalControlActionId) {
            // Manually start the heating rod
            qCDebug(dcMypv()) << "Manually start heating rod";
            // bool power = action.param(elwaExternalControlActionExternalControlParamTypeId).value().toBool();
            bool power = action.params()[0].value().toBool();
            // For ELWA 2, manual needs to be set to 2 to manually actviate boost mode
            // mode = 1 - autoboost
            // mode = 2 - manual boost
            QModbusReply *reply = connection->setManualStart(power ? 2 : 0);
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply, power]() {
                if (reply->error() == QModbusDevice::NoError) {
                    qCDebug(dcMypv()) << "Successfully startet heating rod";
                } else {
                    qCDebug(dcMypv()) << "Error starting heating power";
                }
            });
            thing->setStateValue(elwaExternalControlStateTypeId, power);
            info->finish(Thing::ThingErrorNoError);
        } else {
            Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
        }
    } else {
        Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
    }
}
