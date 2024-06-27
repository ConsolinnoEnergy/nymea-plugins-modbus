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
    const int port = 502;
    const int modbusId = 40;
    qCDebug(dcSax()) << "Checking network device:" << networkDeviceInfo << "Port:" << port << "Modbus ID:" << modbusId;

    SaxModbusTcpConnection *connection = new SaxModbusTcpConnection(networkDeviceInfo.address(), port, modbusId, this);
    connection->setCheckReachableRetries(3);    // For discovery, 3 retries should be enough. Can't set this too high, because it will take too long.
    m_connections.append(connection);

    // I would like to get the test register address from the connection class, but the class does not have a function for that.
    qCDebug(dcSax()) << "Testing connection by trying to read holding register 40072 from " << networkDeviceInfo;

    // "reachable" is tested by trying to read the test register. If an answer is received without errors, "reachable" will change to true.
    connect(connection, &SaxModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Call the update() method of the connection to read all registers. Then check the "frequency" register, to identify the device.
        connection->update();
        connect(connection, &SaxModbusTcpConnection::updateFinished, this, [=](){

            // Note: we get "updateFinished = true" also if all calls failed. So instead test for reachable, because that is false if a call failed.
            if (!connection->reachable()) {
                qCDebug(dcSax()) << "Discovery: Update failed on" << networkDeviceInfo.address().toString();
                cleanupConnection(connection);
                return;
            }

            // Check the frequency to identify this device as a Sax battery. The frequency is always close to 50 Hz.
            qint16 frequencyRegister = connection->frequency();
            qint16 frequencyFactorRegister = connection->frequencyFactor();
            float frequencyValue = frequencyRegister * qPow(10, frequencyFactorRegister);
            if (frequencyValue < 51.0 && frequencyValue > 49.0) {
                float capacityRegister = connection->capacity();
                qCDebug(dcSax()) << "Discovery: --> Found a Sax battery with capacity" << capacityRegister << "kWh at" << networkDeviceInfo.address().toString();
                Result result;
                result.modbusId = modbusId;
                result.port = port;
                result.networkDeviceInfo = networkDeviceInfo;
                result.capacity = capacityRegister;
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcSax()) << "Discovery: --> Found a Modbus device at" << networkDeviceInfo.address().toString()
                                 << ", but it is not a Sax battery. Holding register 40087 should be the grid frequency and holding register 40088 the frequency scaling factor. Register 40087 is"
                                 << frequencyRegister << ", register 40088 is" << frequencyFactorRegister;
            }

            // Nothing left to do with this connection. Clean up.
            cleanupConnection(connection);
        });
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
