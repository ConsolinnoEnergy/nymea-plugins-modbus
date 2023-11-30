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

#ifndef TERRATCPDISCOVERY_H
#define TERRATCPDISCOVERY_H

#include <QObject>
#include <QTimer>

#include <network/networkdevicediscovery.h>

#include "abbmodbustcpconnection.h"

class TerraTCPDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit TerraTCPDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent = nullptr);
    struct Result {
        quint32 firmwareVersion;
        quint16 slaveId;
        NetworkDeviceInfo networkDeviceInfo;
    };

    void startDiscovery();

    QList<Result> discoveryResults() const;

signals:
    void discoveryFinished();

private:
    NetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;

    QTimer m_gracePeriodTimer;
    QDateTime m_startDateTime;

    QList<ABBModbusTcpConnection *> m_connections;

    QList<Result> m_discoveryResults;

    void checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo);
    void cleanupConnection(ABBModbusTcpConnection *connection);

    void finishDiscovery();
};

#endif // TERRATCPDISCOVERY_H
