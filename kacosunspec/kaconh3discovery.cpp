/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2024, nymea GmbH
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

#include "kaconh3discovery.h"
#include "extern-plugininfo.h"

KacoNH3Discovery::KacoNH3Discovery(NetworkDeviceDiscovery *networkDeviceDiscovery, quint16 port, quint16 modbusAddress, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery},
    m_port{port},
    m_modbusAddress{modbusAddress}
{

}

void KacoNH3Discovery::startDiscovery()
{
    qCDebug(dcKacoSunSpec()) << "Discovery: Start searching for Kaco NH3 inverters in the network";
    m_startDateTime = QDateTime::currentDateTime();

    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &KacoNH3Discovery::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, discoveryReply, &NetworkDeviceDiscoveryReply::deleteLater);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=] () {
        qCDebug(dcKacoSunSpec()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this] () {
            qCDebug(dcKacoSunSpec()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
    });
}

QList<KacoNH3Discovery::KacoNH3DiscoveryResult> KacoNH3Discovery::discoveryResults() const
{
    return m_discoveryResults;
}

void KacoNH3Discovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    /* Create a Kaco NH3 connection and try to initialize it.
    * Only if initialized successfully and all information have been fetched correctly from
    * the device we can assume this is what we are locking for (ip, port, modbus address, correct registers).
    */

    qCDebug(dcKacoSunSpec()) << "Creating Kaco NH3 Modbus TCP connection for" << networkDeviceInfo.address() << "Port:" << m_port << "Slave Address" << m_modbusAddress;
    KacoNH3ModbusTcpConnection *connection = new KacoNH3ModbusTcpConnection(networkDeviceInfo.address(), m_port, m_modbusAddress, this);
    connection->setTimeout(5000);
    connection->setNumberOfRetries(0);
    m_connections.append(connection);

    connect(connection, &KacoNH3ModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        qCDebug(dcKacoSunSpec()) << "Kaco NH3 Modbus TCP Connection reachable changed:" << reachable;
        if (!reachable) {
            cleanupConnection(connection);
            return;
        }
        qCDebug(dcKacoSunSpec()) << "Connected, proceeding with initialization";

        connect(connection, &KacoNH3ModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcKacoSunSpec()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString() << "Continue...";
                cleanupConnection(connection);
                return;
            }

            if (connection->deviceModel().contains("NH3")) {
                KacoNH3DiscoveryResult result;
                result.model = connection->deviceModel();
                result.networkDeviceInfo = networkDeviceInfo;
                m_discoveryResults.append(result);
            }

            connection->disconnectDevice();
        });

        qCDebug(dcKacoSunSpec()) << "Discovery: The host" << networkDeviceInfo << "is reachable. Starting with initialization.";
        if (!connection->initialize()) {
            qCDebug(dcKacoSunSpec()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // In case of an error skip the host
    connect(connection, &ModbusTCPMaster::connectionStateChanged, this, [=](bool connected){
        if (connected) {
            qCDebug(dcKacoSunSpec()) << "Discovery: Connected with" << networkDeviceInfo.address().toString() << m_port;
        }
    });

    // In case of an error skip the host
    connect(connection, &ModbusTCPMaster::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcKacoSunSpec()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // If the reachability check failed skip the host
    connect(connection, &KacoNH3ModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcKacoSunSpec()) << "Discovery: Check reachability failed on" << networkDeviceInfo.address().toString() << "Continue...";
        cleanupConnection(connection);
    });

    connection->connectDevice();
}

void KacoNH3Discovery::cleanupConnection(KacoNH3ModbusTcpConnection *connection)
{
    qCDebug(dcKacoSunSpec()) << "Discovery: Cleanup connection";
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void KacoNH3Discovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    foreach (KacoNH3ModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCDebug(dcKacoSunSpec()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count() << "Kaco NH3 inverters in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");
    emit discoveryFinished();
}
