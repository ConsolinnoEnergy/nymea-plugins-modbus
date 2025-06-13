/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#include "discoverytcp.h"
#include "extern-plugininfo.h"

DiscoveryTcp::DiscoveryTcp(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery}
{
}

void DiscoveryTcp::startDiscovery()
{
    qCInfo(dcSiemens()) << "Discovery: Searching for Siemens energy meter in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &DiscoveryTcp::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcSiemens()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this](){
            qCDebug(dcSiemens()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
        discoveryReply->deleteLater();
    });
}

QList<DiscoveryTcp::Result> DiscoveryTcp::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryTcp::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    int port = 502;     // Default modbus port.
    int modbusId = 1;    // The meter responds to any modbus ID on modbus TCP. So the ID does not matter.
    qCDebug(dcSiemens()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << modbusId;

    // This can be improved by first checking if port 502 is open. If that port is not open, it is not a modbus device.

    SiemensPAC2200ModbusTcpConnection *connection = new SiemensPAC2200ModbusTcpConnection(networkDeviceInfo.address(), port, modbusId, this);
    m_connections.append(connection);

    connect(connection, &SiemensPAC2200ModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &SiemensPAC2200ModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcSiemens()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }
            qCDebug(dcSiemens()) << "Initialization success! Reading frequency";

            QModbusReply *reply = connection->readFrequency();
            if (!reply) {
                qCWarning(dcSiemens()) << "Error occurred while reading Mains frequency registers.";
                // Done with this connection
                cleanupConnection(connection);
            }

            if (reply->isFinished()) {
                qCDebug(dcSiemens()) << "Broadcast replay returned. Quit";
                reply->deleteLater(); // Broadcast reply returns immediatly
                // Done with this connection
                cleanupConnection(connection);
            }

            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            connect(reply, &QModbusReply::finished, this, [this, reply, networkDeviceInfo, connection]() {
                qCDebug(dcSiemens()) << "Reply finished";
                if (reply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit unit = reply->result();
                    qCDebug(dcSiemens()) << "Response from Mains frequency register" << 55 << "size:" << 2 << unit.values();
                    float receivedFrequency = ModbusDataUtils::convertToFloat32(unit.values(), ModbusDataUtils::ByteOrderBigEndian);
                    if (receivedFrequency >= 45 && receivedFrequency <= 65) {
                        Result result;
                        result.networkDeviceInfo = networkDeviceInfo;
                        m_discoveryResults.append(result);
                        qCDebug(dcSiemens()) << "Discovery: ----> Found" << result.networkDeviceInfo;
                    } else {
                        qCDebug(dcSiemens()) << "This does not seem like a siemens energy meter";
                    }
                }
                // Done with this connection
                cleanupConnection(connection);
            });

            connect(reply, &QModbusReply::errorOccurred, this, [this, reply, connection] (){
                qCWarning(dcSiemens()) << "Modbus reply error occurred while reading Mains frequency registers from";
                // Done with this connection
                cleanupConnection(connection);
            });
        });

        if (!connection->initialize()) {
            qCDebug(dcSiemens()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &SiemensPAC2200ModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcSiemens()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void DiscoveryTcp::cleanupConnection(SiemensPAC2200ModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void DiscoveryTcp::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (SiemensPAC2200ModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcSiemens()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
                       << "Siemens meter in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}
