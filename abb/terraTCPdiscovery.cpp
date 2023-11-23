/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2023, Consolinno Energy GmbH
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

#include "terraTCPdiscovery.h"
#include "extern-plugininfo.h"

TerraTCPDiscovery::TerraTCPDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery}
{
    m_gracePeriodTimer.setSingleShot(true);
    m_gracePeriodTimer.setInterval(3000);
    connect(&m_gracePeriodTimer, &QTimer::timeout, this, [this](){
        qCDebug(dcAmperfied()) << "Discovery: Grace period timer triggered.";
        finishDiscovery();
    });
}

void TerraTCPDiscovery::startDiscovery()
{
    qCInfo(dcAmperfied()) << "Discovery: Searching for ABB wallboxes in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &TerraTCPDiscovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcAmperfied()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
        m_gracePeriodTimer.start();
        discoveryReply->deleteLater();
    });
}

QList<TerraTCPDiscovery::Result> TerraTCPDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void TerraTCPDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    int port = 502;
    int slaveId = 1;
    qCDebug(dcAmperfied()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << slaveId;

    TerraModbusTcpConnection *connection = new TerraModbusTcpConnection(networkDeviceInfo.address(), port, slaveId, this);
    m_connections.append(connection);

    connect(connection, &TerraModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &TerraModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcAmperfied()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }
            Result result;
            result.firmwareVersion = connection->version();
            result.networkDeviceInfo = networkDeviceInfo;
            m_discoveryResults.append(result);

            qCDebug(dcAmperfied()) << "Discovery: --> Found"
                                << "Version:" << result.firmwareVersion
                                << result.networkDeviceInfo;


            // Done with this connection
            cleanupConnection(connection);
        });

        if (!connection->initialize()) {
            qCDebug(dcAmperfied()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &TerraModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcAmperfied()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void TerraTCPDiscovery::cleanupConnection(TerraModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void TerraTCPDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (TerraModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcAmperfied()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
                       << "ABB wallboxes in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");
    m_gracePeriodTimer.stop();

    emit discoveryFinished();
}
