/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2025, nymea GmbH
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

#ifndef AITDISCOVERY_H
#define AITDISCOVERY_H

#include <QObject>
#include <QTimer>

#include <network/networkdevicediscovery.h>

#include "aitshimodbustcpconnection.h"

class AITDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit AITDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, quint16 port = 502, quint16 modbusAddress = 1, QObject *parent = nullptr);
    typedef struct AitDiscoveryResult {
        NetworkDeviceInfo networkDeviceInfo;
    } AitDiscoveryResult;

    void startDiscovery();

    QList<AitDiscoveryResult> discoveryResults() const;

signals:
    void discoveryFinished();

private:
    NetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;
    quint16 m_port;
    quint16 m_modbusAddress;

    QDateTime m_startDateTime;

    QList<aitShiModbusTcpConnection *> m_connections;
    QList<AitDiscoveryResult> m_discoveryResults;

    void checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo);
    void cleanupConnection(aitShiModbusTcpConnection *connection);

    void finishDiscovery();
};

#endif // AITDISCOVERY_H
