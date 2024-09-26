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
#include "integrationpluginmypv-new.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <network/networkdevicediscovery.h>
#include <QEventLoop>

IntegrationPluginMyPv::IntegrationPluginMyPv()
{
}


void IntegrationPluginMyPv::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == elwaThingClassId) {
        if (!hardwareManager()->networkDeviceDiscovery()->available()) {
            qCWarning(dcMypv()) << "The network discovery is not available on this platform.";
            info->finish(Thing::ThingErrorUnsupportedFeature, QT_TR_NOOP("The network device discovery is not available."));
            return;
        }

        QUdpSocket *searchSocket = new QUdpSocket(this);

        // Note: This will fail, and it's not a problem, but it is required to force the socket to stick to IPv4...
        searchSocket->bind(QHostAddress::AnyIPv4, 16124);

        QByteArray discoveryString;
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

        qCDebug(dcMypv()) << "Discovery is running?" << hardwareManager()->networkDeviceDiscovery()->running();
        NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
        qCDebug(dcMypv()) << "Discovery is running?" << hardwareManager()->networkDeviceDiscovery()->running();

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
                //Device Id Power  = 0x4e8e
                //Device Id AC ELWA-E = 0x3efc
                qCDebug(dcMypv()) << "device Id:" << data.mid(2, 2);
                if (data.mid(2, 2) == QByteArray::fromHex("3efc")) {
                    qCDebug(dcMypv()) << "Found Device: AC ElWA-E";
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x4e8e")) {
                    qCDebug(dcMypv()) << "Found Device: Powermeter";
                } else if (data.mid(2, 2) == QByteArray::fromHex("0x4e84")) {
                    qCDebug(dcMypv()) << "Found Device: AC Thor";
                } else {
                    qCDebug(dcMypv()) << "Failed to parse discovery datagram from" << senderAddress << data;
                    continue;
                }

                ThingDescriptor thingDescriptors(info->thingClassId(), "AC ELWA-E", senderAddress.toString());
                QByteArray serialNumber = data.mid(8, 16);

                foreach (Thing *existingThing, myThings()) {
                    if (serialNumber == existingThing->paramValue(elwaThingSerialNumberParamTypeId).toString()) {
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
                    info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The wallbox was found, but the MAC address is invalid. Try searching again."));
                }

                ParamList params;
                params << Param(elwaThingIpAddressParamTypeId, senderAddress.toString());
                params << Param(elwaThingSerialNumberParamTypeId, serialNumber);
                params << Param(elwaThingMacAddressParamTypeId, heatingRod.macAddress());
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

    if(thing->thingClassId() == elwaThingClassId) {
        // Make sure we have a valid mac address, otherwise no monitor and no auto searching is
        // possible. Testing for null is necessary, because registering a monitor with a zero mac
        // adress will cause a segfault.
        MacAddress macAddress = MacAddress(thing->paramValue(elwaThingMacAddressParamTypeId).toString());
        if (macAddress.isNull()) {
            qCWarning(dcMypv())
                    << "Failed to set up MyPV AC ELWA E because the MAC address is not valid:"
                    << thing->paramValue(elwaThingMacAddressParamTypeId).toString()
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

        qCDebug(dcMypv()) << "Monitor reachable" << monitor->reachable() << thing->paramValue(elwaThingMacAddressParamTypeId).toString();
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
        if (reachable && !thing->stateValue(elwaConnectedStateTypeId).toBool()) {
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
            thing->setStateValue(elwaConnectedStateTypeId, false);
        }
    });

    connect(connection, &MyPvModbusTcpConnection::initializationFinished, thing, [this, connection, thing] (bool success) {
        thing->setStateValue(elwaConnectedStateTypeId, success);
        if (success) {
            qCDebug(dcMypv()) << "my-PV AC ELWA intialized.";
        } else {
            qCDebug(dcMypv()) << "my-PV AC ELWA initialization failed.";
            connection->reconnectDevice();
        }
    });

    connect(connection, &MyPvModbusTcpConnection::currentPowerChanged, thing, [thing](quint16 power) {
        qCDebug(dcMypv()) << "Current power changed" << power << "W";
        thing->setStateValue(elwaCurrentPowerStateTypeId, power);
    });

    connect(connection, &MyPvModbusTcpConnection::waterTemperatureChanged, thing, [thing](double temp) {
        qCDebug(dcMypv()) << "Actual water temperature changed" << temp << "°C";
        thing->setStateValue(elwaTemperatureStateTypeId, temp);
    });

    connect(connection, &MyPvModbusTcpConnection::targetWaterTemperatureChanged, thing, [thing](double temp) {
        qCDebug(dcMypv()) << "Target water temperature changed" << temp << "°C";
        thing->setStateValue(elwaTargetWaterTemperatureStateTypeId, temp);
    });

    connect(connection, &MyPvModbusTcpConnection::elwaStatusChanged, thing, [thing](quint16 state) {
        qCDebug(dcMypv()) << "State changed" << state;
        switch (state) {
        case MyPvModbusTcpConnection::ElwaStatusHeating:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Heating"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusStandby:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Standby"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusBoosted:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Boosted"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusHeatFinished:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Heating Finished"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusSetup:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Setup"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempFuseBlown:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Error Overtemp Fuse Blown"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempMeasured:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Error Overtemp Measured"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorOvertempElectronics:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Error Overtemp Electronics"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorHardwareFault:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Error Hardware Fault"));
            break;
        case MyPvModbusTcpConnection::ElwaStatusErrorTempSensor:
            thing->setStateValue(elwaStatusStateTypeId, QT_TR_NOOP("Error Temperatur Sensor"));
            break;
        }
    });

    if (monitor->reachable())
        connection->connectDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginMyPv::postSetupThing(Thing *thing)
{
    Q_UNUSED(thing)
    if (!m_refreshTimer) {
        qCDebug(dcMypv()) << "Starting plugin timer";
        m_refreshTimer = hardwareManager()->pluginTimerManager()->registerTimer(10);
        connect(m_refreshTimer, &PluginTimer::timeout, this, [this] {
            foreach(MyPvModbusTcpConnection *connection, m_tcpConnections) {
                qCDebug(dcMypv()) << "Updated heating rod";
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

// void IntegrationPluginMyPv::executeAction(ThingActionInfo *info)
// {
//     Thing *thing = info->thing();
//     Action action = info->action();
// 
//     if (thing->thingClassId() == elwaThingClassId) {
// 
//         ModbusTCPMaster *modbusTCPMaster = m_modbusTcpMasters.value(thing);
//         if (action.actionTypeId() == elwaHeatingPowerActionTypeId) {
//             int heatingPower = action.param(elwaHeatingPowerActionHeatingPowerParamTypeId).value().toInt();
//             QUuid requestId = modbusTCPMaster->writeHoldingRegister(0xff, ElwaModbusRegisters::Power, heatingPower);
//             if (requestId.isNull()) {
//                 info->finish(Thing::ThingErrorHardwareNotAvailable);
//             } else {
//                 m_asyncActions.insert(requestId, info);
//                 connect(info, &ThingActionInfo::aborted, this, [this, requestId] {m_asyncActions.remove(requestId);});
//             }
//         } else if (action.actionTypeId() == elwaPowerActionTypeId) {
//             bool power = action.param(elwaHeatingPowerActionHeatingPowerParamTypeId).value().toBool();
//             if(power) {
//                 QUuid requestId = modbusTCPMaster->writeHoldingRegister(0xff, ElwaModbusRegisters::ManuelStart, 1);
//                 if (requestId.isNull()) {
//                     info->finish(Thing::ThingErrorHardwareNotAvailable);
//                 } else {
//                     m_asyncActions.insert(requestId, info);
//                     connect(info, &ThingActionInfo::aborted, this, [this, requestId] {m_asyncActions.remove(requestId);});
//                 }
//             }
//         } else {
//             Q_ASSERT_X(false, "executeAction", QString("Unhandled actionTypeId: %1").arg(action.actionTypeId().toString()).toUtf8());
//         }
//     } else {
//         Q_ASSERT_X(false, "executeAction", QString("Unhandled thingClassId: %1").arg(thing->thingClassId().toString()).toUtf8());
//     }
// }

// void IntegrationPluginMyPv::onConnectionStateChanged(bool status)
// {
//     ModbusTCPMaster *modbusTcpMaster = static_cast<ModbusTCPMaster *>(sender());
//     Thing *thing = m_modbusTcpMasters.key(modbusTcpMaster);
//     if (!thing)
//         return;
//     thing->setStateValue(elwaConnectedStateTypeId, status);
// }
// 
// void IntegrationPluginMyPv::onWriteRequestExecuted(QUuid requestId, bool success)
// {
//     if (m_asyncActions.contains(requestId)) {
//         ThingActionInfo *info = m_asyncActions.value(requestId);
//         if (success) {
//             info->finish(Thing::ThingErrorNoError);
//         } else {
//             info->finish(Thing::ThingErrorHardwareNotAvailable);
//         }
//     }
// }
// 
// void IntegrationPluginMyPv::onReceivedHoldingRegister(quint32 slaveAddress, quint32 modbusRegister, const QVector<quint16> &value)
// {
//     Q_UNUSED(slaveAddress)
//     ModbusTCPMaster *modbusTcpMaster = static_cast<ModbusTCPMaster *>(sender());
//     Thing *thing = m_modbusTcpMasters.key(modbusTcpMaster);
//     if (!thing)
//         return;
// 
//     if(modbusRegister == ElwaModbusRegisters::Status) {
//         switch (ElwaStatus(value[0])) {
//         case ElwaStatus::Heating: {
//             thing->setStateValue(elwaStatusStateTypeId, "Heating");
//             thing->setStateValue(elwaPowerStateTypeId, true);
//             break;
//         }
//         case ElwaStatus::Standby:{
//             thing->setStateValue(elwaStatusStateTypeId, "Standby");
//             thing->setStateValue(elwaPowerStateTypeId, false);
//             break;
//         }
//         case ElwaStatus::Boosted:{
//             thing->setStateValue(elwaStatusStateTypeId, "Boosted");
//             thing->setStateValue(elwaPowerStateTypeId, true);
//             break;
//         }
//         case ElwaStatus::HeatFinished:{
//             thing->setStateValue(elwaStatusStateTypeId, "Heat finished");
//             thing->setStateValue(elwaPowerStateTypeId, false);
//             break;
//         }
//         case ElwaStatus::Setup:{
//             thing->setStateValue(elwaStatusStateTypeId, "Setup");
//             thing->setStateValue(elwaPowerStateTypeId, false);
//             break;
//         }
//         case ElwaStatus::ErrorOvertempFuseBlown:{
//             thing->setStateValue(elwaStatusStateTypeId, "Error Overtemp Fuse blown");
//             break;
//         }
//         case ElwaStatus::ErrorOvertempMeasured:{
//             thing->setStateValue(elwaStatusStateTypeId, "Error Overtemp measured");
//             break;
//         }
//         case ElwaStatus::ErrorOvertempElectronics:{
//             thing->setStateValue(elwaStatusStateTypeId, "Error Overtemp Electronics");
//             break;
//         }
//         case ElwaStatus::ErrorHardwareFault:{
//             thing->setStateValue(elwaStatusStateTypeId, "Error Hardware Fault");
//             break;
//         }
//         case ElwaStatus::ErrorTempSensor:{
//             thing->setStateValue(elwaStatusStateTypeId, "Error Temp Sensor");
//             break;
//         }
//         default:
//             thing->setStateValue(elwaStatusStateTypeId, "Unknown");
//         }
//     } else if(modbusRegister == ElwaModbusRegisters::WaterTemperature) {
//         thing->setStateValue(elwaTemperatureStateTypeId, value[0]/10.00);
//     } else if(modbusRegister == ElwaModbusRegisters::TargetWaterTemperature) {
//         thing->setStateValue(elwaTargetWaterTemperatureStateTypeId, value[0]/10.00);
//     } else if(modbusRegister == ElwaModbusRegisters::Power) {
//         thing->setStateValue(elwaHeatingPowerStateTypeId, value[0]);
//     } else {
//         qCWarning(dcMypv()) << "Received unhandled modbus register";
//     }
// }
// 
// void IntegrationPluginMyPv::update(Thing *thing)
// {
//     if (thing->thingClassId() == elwaThingClassId) {
//         ModbusTCPMaster *modbusTCPMaster = m_modbusTcpMasters.value(thing);
// 
//         modbusTCPMaster->readHoldingRegister(0xff, ElwaModbusRegisters::Status);
//         modbusTCPMaster->readHoldingRegister(0xff, ElwaModbusRegisters::WaterTemperature);
//         modbusTCPMaster->readHoldingRegister(0xff, ElwaModbusRegisters::TargetWaterTemperature);
//         modbusTCPMaster->readHoldingRegister(0xff, ElwaModbusRegisters::Power);
//     }
// }
// 
