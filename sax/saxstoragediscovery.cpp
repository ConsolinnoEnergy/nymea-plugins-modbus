/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2022 - 2023, Consolinno Energy GmbH
* Contact: info@consolinno.de
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
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "saxstoragediscovery.h"
#include "extern-plugininfo.h"

SaxStorageDiscovery::SaxStorageDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery}
{
    m_gracePeriodTimer.setSingleShot(true);
    m_gracePeriodTimer.setInterval(3000);
    connect(&m_gracePeriodTimer, &QTimer::timeout, this, [this](){
        qCDebug(dcSax()) << "Discovery: Grace period timer triggered.";
        finishDiscovery();
    });
}

void SaxStorageDiscovery::startDiscovery()
{
    qCInfo(dcSax()) << "Discovery: Searching for Sax storages in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &SaxStorageDiscovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcSax()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
        m_gracePeriodTimer.start();
        discoveryReply->deleteLater();
    });
}

QList<SaxStorageDiscovery::Result> SaxStorageDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void SaxStorageDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    int port = 502;
    int slaveId = 1;
    qCDebug(dcSax()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << slaveId;

    SaxModbusTcpConnection *connection = new SaxModbusTcpConnection(networkDeviceInfo.address(), port, slaveId, this);
    m_connections.append(connection);

    connect(connection, &SaxModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &SaxModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcSax()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }
            Result result;

            result.capacity_register = connection->capacity();

            result.networkDeviceInfo = networkDeviceInfo;
            m_discoveryResults.append(result);         
            qCDebug(dcSax()) << "Discovery: --> Found Modbus Device:" 
                                << result.networkDeviceInfo;         



            // Done with this connection
            cleanupConnection(connection);
        });

        if (!connection->initialize()) {
            qCDebug(dcSax()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &SaxModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcSax()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void SaxStorageDiscovery::cleanupConnection(SaxModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void SaxStorageDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (SaxModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcSax()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
                       << "Sax storage in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");
    m_gracePeriodTimer.stop();

    emit discoveryFinished();
}
