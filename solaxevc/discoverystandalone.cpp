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

#include "discoverystandalone.h"
#include "extern-plugininfo.h"

SolaxEvcStandaloneDiscovery::SolaxEvcStandaloneDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery,
                                           QObject *parent)
    : QObject{ parent }, m_networkDeviceDiscovery{ networkDeviceDiscovery }
{
}

void SolaxEvcStandaloneDiscovery::startDiscovery()
{
    qCInfo(dcSolaxEvc()) << "Discovery: Searching for Solax wallboxes in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this,
            &SolaxEvcStandaloneDiscovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcSolaxEvc()) << "Discovery: Network discovery finished. Found"
                              << discoveryReply->networkDeviceInfos().count() << "network devices";

        // Give the last connections added right before the network discovery finished a chance to
        // check the device...
        QTimer::singleShot(3000, this, [this]() {
            qCDebug(dcSolaxEvc()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
        discoveryReply->deleteLater();
    });
}

QList<SolaxEvcStandaloneDiscovery::Result> SolaxEvcStandaloneDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void SolaxEvcStandaloneDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    int port = 502;
    int modbusId = 1;
    qCDebug(dcSolaxEvc()) << "Checking network device:" << networkDeviceInfo << "Port:" << port
                          << "Slave ID:" << modbusId;

    SolaxEvcStandaloneModbusTcpConnection *connection =
            new SolaxEvcStandaloneModbusTcpConnection(networkDeviceInfo.address(), port, modbusId, this);
    m_connections.append(connection);

    connect(connection, &SolaxEvcStandaloneModbusTcpConnection::reachableChanged, this, [=](bool reachable) {
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &SolaxEvcStandaloneModbusTcpConnection::initializationFinished, this,
                [=](bool success) {
                    if (!success) {
                        qCDebug(dcSolaxEvc()) << "Discovery: Initialization failed on"
                                              << networkDeviceInfo.address().toString();
                        cleanupConnection(connection);
                        return;
                    }

                    if (connection->firmwareVersion() >= 112) {
                        Result result;
                        result.firmwareVersion = connection->firmwareVersion();
                        result.port = port;
                        result.networkDeviceInfo = networkDeviceInfo;
                        result.modbusId = modbusId;
                        m_discoveryResults.append(result);

                        qCDebug(dcSolaxEvc())
                                << "Discovery: --> Found Version:" << result.firmwareVersion
                                << result.networkDeviceInfo;
                    } else {
                        qCDebug(dcSolaxEvc()) << "Firmware version not correct!";
                    }

                    // Done with this connection
                    cleanupConnection(connection);
                });

        if (!connection->initialize()) {
            qCDebug(dcSolaxEvc()) << "Discovery: Unable to initialize connection on"
                                  << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &SolaxEvcStandaloneModbusTcpConnection::checkReachabilityFailed, this, [=]() {
        qCDebug(dcSolaxEvc()) << "Discovery: Checking reachability failed on"
                              << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    connect(connection, &ModbusTcpMaster::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcSolaxEvc()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";
            cleanupConnection(connection);
        }
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void SolaxEvcStandaloneDiscovery::cleanupConnection(SolaxEvcStandaloneModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void SolaxEvcStandaloneDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds =
            QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (SolaxEvcStandaloneModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcSolaxEvc())
            << "Discovery: Finished the discovery process. Found" << m_discoveryResults.count()
            << "Solax wallboxes in"
            << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");

    emit discoveryFinished();
}
