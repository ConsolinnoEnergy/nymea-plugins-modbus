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

#ifndef DISCOVERYTCP_H
#define DISCOVERYTCP_H

#include <QObject>
#include <QTimer>

#include <network/networkdevicediscovery.h>

#include "solaxevcmodbustcpconnection.h"

class SolaxEvcTCPDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit SolaxEvcTCPDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery,
                                  QObject *parent = nullptr);
    struct Result
    {
        quint16 firmwareVersion;
        quint16 modbusId;
        quint16 port;
        NetworkDeviceInfo networkDeviceInfo;
    };

    void startDiscovery();

    QList<Result> discoveryResults() const;

signals:
    void discoveryFinished();

private:
    NetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;

    QDateTime m_startDateTime;

    QList<SolaxEvcModbusTcpConnection *> m_connections;

    QList<Result> m_discoveryResults;

    void checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo);
    void cleanupConnection(SolaxEvcModbusTcpConnection *connection);

    void finishDiscovery();
};

#endif // DISCOVERYTCP_H
