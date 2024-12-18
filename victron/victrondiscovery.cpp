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
    qCInfo(dcVictron()) << "Discovery: Start searching for Victron inverters in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    // Imedialty check any new device gets discovered
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &VictronDiscovery::checkNetworkDevice);

    // Check what might be left on finished
        connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcVictron()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
        m_networkDeviceInfos = discoveryReply->networkDeviceInfos();

        // Send a report request to nework device info not sent already...
        foreach (const NetworkDeviceInfo &networkDeviceInfo, m_networkDeviceInfos) {
            if (!m_verifiedNetworkDeviceInfos.contains(networkDeviceInfo)) {
                checkNetworkDevice(networkDeviceInfo);
            }
        }

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this](){
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
    // We cloud tough also filter the result only for certain software versions, manufactueres or whatever...

    if (m_verifiedNetworkDeviceInfos.contains(networkDeviceInfo))
        return;

    VictronModbusTcpConnection *connection = new VictronModbusTcpConnection(networkDeviceInfo.address(), m_port, m_modbusAddress, this);
    m_connections.append(connection);
    m_verifiedNetworkDeviceInfos.append(networkDeviceInfo);

    connect(connection, &VictronModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &VictronModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcVictron()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString() << "Continue...";;
                cleanupConnection(connection);
                return;
            }

            VictronDiscoveryResult result;
            result.productName = connection->productName();
            result.manufacturerName = connection->inverterManufacturer();
            result.serialNumber = connection->inverterSerialNumber1();
            result.articleNumber = connection->inverterArticleNumber();
            result.softwareVersionIoController = connection->softwareVersionIoController();
            result.softwareVersionMainController = connection->softwareVersionMainController();
            result.networkDeviceInfo = networkDeviceInfo;
            m_discoveryResults.append(result);

            qCDebug(dcVictron()) << "Discovery: --> Found" << result.manufacturerName << result.productName
                                << "Article:" << result.articleNumber
                                << "Serial number:" << result.serialNumber
                                << "Software version main controller:" << result.softwareVersionMainController
                                << "Software version IO controller:" << result.softwareVersionIoController
                                << result.networkDeviceInfo;


            // Done with this connection
            cleanupConnection(connection);
        });

        // Initializing...
        if (!connection->initialize()) {
            qCDebug(dcVictron()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString() << "Continue...";;
            cleanupConnection(connection);
        }
    });

    // If we get any error...skip this host...
    connect(connection, &VictronModbusTcpConnection::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcVictron()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";;
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &VictronModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcVictron()) << "Discovery: Check reachability failed on" << networkDeviceInfo.address().toString() << "Continue...";;
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void VictronDiscovery::cleanupConnection(VictronModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void VictronDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (VictronModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcVictron()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count() << "Victron Inverters in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}
