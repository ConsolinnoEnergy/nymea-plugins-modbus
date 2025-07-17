/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2025, nymea GmbH
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

#include "aitdiscovery.h"
#include "extern-plugininfo.h"

AITDiscovery::AITDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, quint16 port, quint16 modbusAddress, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery},
    m_port{port},
    m_modbusAddress{modbusAddress}
{

}

void AITDiscovery::startDiscovery()
{
    qCDebug(dcAlphaInnotec()) << "Discovery: Start searching for AIT Heatpumps in the network";
    m_startDateTime = QDateTime::currentDateTime();

    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &AITDiscovery::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=] () {
        qCDebug(dcAlphaInnotec()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this] () {
            qCDebug(dcAlphaInnotec()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
    });
}

QList<AITDiscovery::AitDiscoveryResult> AITDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void AITDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    /* Create a AIT connection and try to initialize it.
    * Only if initialized successfully and all information have been fetched correctly from
    * the device we can assume this is what we are locking for (ip, port, modbus address, correct registers).
    */

    qCDebug(dcAlphaInnotec()) << "Creating AIT SHI Modbus TCP connection for" << networkDeviceInfo.address() << "Port:" << m_port << "Slave Address" << m_modbusAddress;
    aitShiModbusTcpConnection *connection = new aitShiModbusTcpConnection(networkDeviceInfo.address(), m_port, m_modbusAddress, this);
    connection->modbusTcpMaster()->setTimeout(5000);
    connection->modbusTcpMaster()->setNumberOfRetries(0);
    m_connections.append(connection);

    connect(connection, &aitShiModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        qCDebug(dcAlphaInnotec()) << "AIT Modbus TCP Connection reachable changed:" << reachable;
        if (!reachable) {
            cleanupConnection(connection);
            return;
        }
        qCDebug(dcAlphaInnotec()) << "Connected, proceeding with initialization";

        connect(connection, &aitShiModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcAlphaInnotec()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString() << "Continue...";
                cleanupConnection(connection);
                return;
            }

            if (connection->softwarePlatform() >= 3 &&  connection->majorVersion() >= 90) {
                AitDiscoveryResult result;
                result.networkDeviceInfo = networkDeviceInfo;
                m_discoveryResults.append(result);
            }

            connection->disconnectDevice();
        });

        qCDebug(dcAlphaInnotec()) << "Discovery: The host" << networkDeviceInfo << "is reachable. Starting with initialization.";
        if (!connection->initialize()) {
            qCDebug(dcAlphaInnotec()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // In case of an error skip the host
    connect(connection->modbusTcpMaster(), &ModbusTcpMaster::connectionStateChanged, this, [=](bool connected){
        if (connected) {
            qCDebug(dcAlphaInnotec()) << "Discovery: Connected with" << networkDeviceInfo.address().toString() << m_port;
        }
    });

    // In case of an error skip the host
    connect(connection->modbusTcpMaster(), &ModbusTcpMaster::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcAlphaInnotec()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // If the reachability check failed skip the host
    connect(connection, &aitShiModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcAlphaInnotec()) << "Discovery: Check reachability failed on" << networkDeviceInfo.address().toString() << "Continue...";
        cleanupConnection(connection);
    });

    connection->connectDevice();
}

void AITDiscovery::cleanupConnection(aitShiModbusTcpConnection *connection)
{
    qCDebug(dcAlphaInnotec()) << "Discovery: Cleanup connection";
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void AITDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    foreach (aitShiModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCDebug(dcAlphaInnotec()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count() << "AIT Heatpumps in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");
    emit discoveryFinished();
}
