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

#include "discovery.h"
#include "extern-plugininfo.h"

#include <QTcpSocket>

Discovery::Discovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery}
{}

void Discovery::startDiscovery()
{
    qCInfo(dcStiebelEltron()) << "Discovery: Searching for Stiebel Eltron heat pumps in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    m_openReplies = 0;
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &Discovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        if (m_openReplies < 0) {
            m_openReplies = 0;
        }
        if (m_openReplies) {
            qCDebug(dcStiebelEltron()) << "Waiting for modbus TCP replies.";
            connect(this, &Discovery::repliesFinished, this, [=](){
                qCDebug(dcStiebelEltron()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
                emit discoveryFinished();
            });
        } else {
            qCDebug(dcStiebelEltron()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
            emit discoveryFinished();
        }
        discoveryReply->deleteLater();
    });
}

QList<Discovery::Result> Discovery::discoveryResults() const
{
    return m_discoveryResults;
}

void Discovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    // Track that we are checking a device.
    m_openReplies++;

    // First, check if the Modbus port is open. That fails faster than trying to set up a modbus connection.
    QTcpSocket *socket = new QTcpSocket(this);
    socket->connectToHost(networkDeviceInfo.address(), 502, QIODevice::ReadWrite);

    if (socket->waitForConnected(200)) {
        socket->disconnectFromHost();

        qCDebug(dcStiebelEltron()) << "Discovery: port 502 of address" << networkDeviceInfo.address().toString() << "is open. Proceeding to start Modbus communication.";

        int port = 502;
        int slaveId = 1;
        qCDebug(dcStiebelEltron()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << slaveId;

        TestModbusTcpConnection *connection = new TestModbusTcpConnection(networkDeviceInfo.address(), port, slaveId, this);
        m_connections.append(connection);

        connect(connection, &TestModbusTcpConnection::reachableChanged, this, [=](bool reachable){
            if (!reachable) {
                // Disconnected ... done with this connection
                cleanupConnection(connection);
                return;
            }

            // Modbus TCP connected...ok, let's try to initialize it!
            connect(connection, &TestModbusTcpConnection::initializationFinished, this, [=](bool success){
                if (!success) {
                    qCDebug(dcStiebelEltron()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                    cleanupConnection(connection);
                    return;
                }

                uint controllerType = connection->controllerType();
                switch (controllerType) {
                case 103:
                case 104:
                case 390:
                case 391:
                case 449:
                    qCDebug(dcStiebelEltron()) << "Discovery: Modbus reply for input register 5001 is" << controllerType << ", this seems to be a Stiebel Eltron heat pump" << networkDeviceInfo.address().toString();
                    break;
                default:
                    qCDebug(dcStiebelEltron()) << "Discovery: Modbus reply for input register 5001 is" << controllerType << ". This value is not in the database, skipping this device" << networkDeviceInfo.address().toString();
                    cleanupConnection(connection);
                    return;
                }

                Result result;
                result.controllerType = controllerType;
                result.networkDeviceInfo = networkDeviceInfo;
                m_discoveryResults.append(result);

                // Done with this connection
                cleanupConnection(connection);
            });

            if (!connection->initialize()) {
                qCDebug(dcStiebelEltron()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
            }
        });

        // If check reachability failed...skip this host...
        connect(connection, &TestModbusTcpConnection::checkReachabilityFailed, this, [=](){
            qCDebug(dcStiebelEltron()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        });

        // Try to connect, maybe it works, maybe not...
        connection->connectDevice();
    } else {
        qCDebug(dcStiebelEltron()) << "Discovery: port 502 of address" << networkDeviceInfo.address().toString() << "is closed. This device does not have Modbus enabled.";

        m_openReplies--;
        if (m_openReplies <= 0) {
            emit repliesFinished();
        }
    }
    socket->deleteLater();
}

void Discovery::cleanupConnection(TestModbusTcpConnection *connection)
{
    m_openReplies--;
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
    if (m_openReplies <= 0) {
        emit repliesFinished();
    }
}
