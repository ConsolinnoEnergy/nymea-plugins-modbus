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

#include "discoverytcp.h"
#include "extern-plugininfo.h"

SolaxEvcTCPDiscovery::SolaxEvcTCPDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent) :
    QObject{parent},
    m_networkDeviceDiscovery{networkDeviceDiscovery}
{
}

void SolaxEvcTCPDiscovery::startDiscovery()
{
    qCInfo(dcSolaxEvc()) << "Discovery: Searching for Solax wallboxes in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &SolaxEvcTCPDiscovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcSolaxEvc()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this](){
            qCDebug(dcSolaxEvc()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
        discoveryReply->deleteLater();
    });
}

QList<SolaxEvcTCPDiscovery::Result> SolaxEvcTCPDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void SolaxEvcTCPDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    int port = 502;
    int modbusId = 1;
    qCDebug(dcSolaxEvc()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Slave ID:" << modbusId;

    SolaxEvcModbusTcpConnection *connection = new SolaxEvcModbusTcpConnection(networkDeviceInfo.address(), port, modbusId, this);
    m_connections.append(connection);

    connect(connection, &SolaxEvcModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &SolaxEvcModbusTcpConnection::initializationFinished, this, [=](bool success){
            if (!success) {
                qCDebug(dcSolaxEvc()) << "Discovery: Initialization failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }

            // TODO: CHECK A REGISTER
 
            if (connection->firmwareVersion() > 0)
            {
                Result result;
                result.firmwareVersion = connection->firmwareVersion();
                result.networkDeviceInfo = networkDeviceInfo;
                m_discoveryResults.append(result);

                qCDebug(dcSolaxEvc()) << "Discovery: --> Found Version:" 
                                    << result.firmwareVersion
                                    << result.networkDeviceInfo;
            } else {
                qCDebug(dcSolaxEvc()) << "Firmware version not correct!";
            }

            // Done with this connection
            cleanupConnection(connection);
        });

        if (!connection->initialize()) {
            qCDebug(dcSolaxEvc()) << "Discovery: Unable to initialize connection on" << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &SolaxEvcModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcSolaxEvc()) << "Discovery: Checking reachability failed on" << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void SolaxEvcTCPDiscovery::cleanupConnection(SolaxEvcModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void SolaxEvcTCPDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (SolaxEvcModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcSolaxEvc()) << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
                       << "Solax wallboxes in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}

