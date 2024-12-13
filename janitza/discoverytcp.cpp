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
    qCInfo(dcJanitza()) << "Discovery: Searching for Janitza energy meter in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &DiscoveryTcp::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcJanitza()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this](){
            qCDebug(dcJanitza()) << "Discovery: Grace period timer triggered.";
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
    int modbusId = 1;    // The solax responds to any modbus ID on modbus TCP. So the ID does not matter.
    qCDebug(dcJanitza()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << modbusId;

    // This can be improved by first checking if port 502 is open. If that port is not open, it is not a modbus device.

    umg604ModbusTcpConnection *connection = new umg604ModbusTcpConnection(networkDeviceInfo.address(), port, modbusId, this);
    m_connections.append(connection);

    connect(connection, &umg604ModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &umg604ModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcJanitza()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }

            quint16 frequency = connection->frequency();

            bool frequencyIsInRange = false;
            if (frequency > 49.0 || frequency < 51.0) {
                qCDebug(dcJanitza()) << "Recieved value for grid frequency is" << frequency << "Hz.";
                frequencyIsInRange = true;
            }

            bool firmwareIsCorrect = false;
            QString firmware = connection->firmwareVersion();
            if (firmware == "5.029") {
                qCDebug(dcJanitza()) << "Received firmware version" << firmware << "is correct";
                firmwareIsCorrect = true;
            }


            if (frequencyIsInRange && firmwareIsCorrect) {
                Result result;
                result.port = port;
                result.firmwareVersion = firmware;
                result.serialNumber = connection->serialNumber();
                result.networkDeviceInfo = networkDeviceInfo;
                m_discoveryResults.append(result);
                qCDebug(dcJanitza()) << "Discovery: --> Found" << result.networkDeviceInfo;
            } else {
                qCDebug(dcJanitza()) << QString("This does not seem like a Janitza energy meter.");
            }

            // Done with this connection
            cleanupConnection(connection);
        });

        if (!connection->initialize()) {
            qCDebug(dcJanitza()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &umg604ModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcJanitza()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void DiscoveryTcp::cleanupConnection(umg604ModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void DiscoveryTcp::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (umg604ModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcJanitza()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
                       << "Janitza meter in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}
