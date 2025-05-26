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

#include "victrondiscovery.h"
#include "extern-plugininfo.h"

VictronDiscovery::VictronDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, quint16 port, quint16 modbusAddress, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery},
    m_port{port},
    m_modbusAddress{modbusAddress}
{

}

void VictronDiscovery::startDiscovery()
{
    qCDebug(dcVictron()) << "Discovery: Start searching for Victron inverters in the network...";
    m_startDateTime = QDateTime::currentDateTime();

    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &VictronDiscovery::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=] () {
        qCDebug(dcVictron()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this] () {
            qCDebug(dcVictron()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
    });
}

QList<VictronDiscovery::VictronDiscoveryResult> VictronDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void VictronDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    // Create a victron connection and try to initialize it.
    // Only if initialized successfully and all information have been fetched correctly from
    // the device we can assume this is what we are locking for (ip, port, modbus address, correct registers).

    qCDebug(dcVictron()) << "Creating Victron Modbus TCP connection for" << networkDeviceInfo.address() << "Port:" << m_port << "Slave Address" << m_modbusAddress;
    VictronSystemModbusTcpConnection *connection = new VictronSystemModbusTcpConnection(networkDeviceInfo.address(), m_port, m_modbusAddress, this);
    connection->setTimeout(5000);
    connection->setNumberOfRetries(0);
    m_connections.append(connection);

    connect(connection, &VictronSystemModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        qCDebug(dcVictron()) << "Victron Modbus TCP Connection reachable changed:" << reachable;
        if (!reachable) {
            cleanupConnection(connection);
            return;
        }
        qCDebug(dcVictron()) << "Connected, proceeding with initialization";

        connect(connection, &VictronSystemModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcVictron()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString() << "Continue...";;
                cleanupConnection(connection);
                return;
            }
	    
            qCDebug(dcVictron()) << "Discovery: Initialized successfully" << networkDeviceInfo << connection->serialNumber();

            VictronDiscoveryResult result;
            result.networkDeviceInfo = networkDeviceInfo;
            result.serialNumber = connection->serialNumber();
            m_discoveryResults.append(result);

            connection->disconnectDevice();
        });

        qCDebug(dcVictron()) << "Discovery: The host" << networkDeviceInfo << "is reachable. Starting with initialization.";
        if (!connection->initialize()) {
            qCDebug(dcVictron()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // In case of an error skip the host
    connect(connection, &ModbusTCPMaster::connectionStateChanged, this, [=](bool connected){
        if (connected) {
            qCDebug(dcVictron()) << "Discovery: Connected with" << networkDeviceInfo.address().toString() << m_port;
        }
    });

    // In case of an error skip the host
    connect(connection, &ModbusTCPMaster::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcVictron()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // If the reachability check failed skip the host
    connect(connection, &VictronSystemModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcVictron()) << "Discovery: Check reachability failed on" << networkDeviceInfo.address().toString() << "Continue...";
        cleanupConnection(connection);
    });

    connection->connectDevice();
}

void VictronDiscovery::cleanupConnection(VictronSystemModbusTcpConnection *connection)
{
    qCDebug(dcVictron()) << "Discovery: Cleanup connection";
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void VictronDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    foreach (VictronSystemModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCDebug(dcVictron()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count() << "Victron Inverters in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}
