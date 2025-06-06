/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright 2025, Consolinno Energy GmbH
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

SolaxEvcG2TCPDiscovery::SolaxEvcG2TCPDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery,
                                               QObject *parent)
    : QObject{ parent }
    , m_networkDeviceDiscovery{ networkDeviceDiscovery }
{
}

void SolaxEvcG2TCPDiscovery::startDiscovery()
{
    qCInfo(dcSolaxEvcG2()) << "Discovery: Searching for Solax wallboxes in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this,
            &SolaxEvcG2TCPDiscovery::checkNetworkDevice);

    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=]() {
        qCDebug(dcSolaxEvcG2())
                << "Discovery: Network discovery finished. Found"
                << discoveryReply->networkDeviceInfos().count()
                << "network devices";

        // Give the last connections added right before the network discovery finished a chance to
        // check the device...
        QTimer::singleShot(3000, this, [this]() {
            qCDebug(dcSolaxEvcG2()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
        discoveryReply->deleteLater();
    });
}

QList<SolaxEvcG2TCPDiscovery::Result> SolaxEvcG2TCPDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void SolaxEvcG2TCPDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    uint port = 502;
    quint16 modbusId = 1;
    qCDebug(dcSolaxEvcG2())
            << "Checking network device:"
            << networkDeviceInfo
            << "Port:" << port
            << "Slave ID:"
            << modbusId;

    SolaxEvcG2ModbusTcpConnection *connection =
            new SolaxEvcG2ModbusTcpConnection{ networkDeviceInfo.address(), port, modbusId, this };
    m_connections.append(connection);

    connect(connection, &SolaxEvcG2ModbusTcpConnection::reachableChanged, this, [=](bool reachable) {
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Modbus TCP connected...ok, let's try to initialize it!
        connect(connection, &SolaxEvcG2ModbusTcpConnection::initializationFinished, this,
                [=](bool success) {
            if (!success) {
                qCDebug(dcSolaxEvcG2())
                        << "Discovery: Initialization failed on"
                        << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }

            // #TODO minimum firmware like in solaxevc plugin?
            if (connection->firmwareVersion() >= 1) {
                Result result;
                result.firmwareVersion = connection->firmwareVersion();
                result.port = port;
                result.networkDeviceInfo = networkDeviceInfo;
                result.modbusId = modbusId;
                m_discoveryResults.append(result);

                qCDebug(dcSolaxEvcG2())
                        << "Discovery: --> Found Version:" << result.firmwareVersion
                        << result.networkDeviceInfo;
            } else {
                qCDebug(dcSolaxEvcG2()) << "Firmware version too low!";
            }

            // Done with this connection
            cleanupConnection(connection);
        });

        if (!connection->initialize()) {
            qCDebug(dcSolaxEvcG2())
                    << "Discovery: Unable to initialize connection on"
                    << networkDeviceInfo.address().toString();
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &SolaxEvcG2ModbusTcpConnection::checkReachabilityFailed, this, [=]() {
        qCDebug(dcSolaxEvcG2())
                << "Discovery: Checking reachability failed on"
                << networkDeviceInfo.address().toString();
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void SolaxEvcG2TCPDiscovery::cleanupConnection(SolaxEvcG2ModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void SolaxEvcG2TCPDiscovery::finishDiscovery()
{
    // Cleanup any leftovers...we don't care any more
    foreach (SolaxEvcG2ModbusTcpConnection *connection, m_connections) {
        cleanupConnection(connection);
    }
    qCInfo(dcSolaxEvcG2())
            << "Discovery: Finished the discovery process. Found"
            << m_discoveryResults.count()
            << "Solax wallboxes";
    emit discoveryFinished();
}
