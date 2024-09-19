/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2023, nymea GmbH
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

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QObject>

#include <network/networkdevicediscovery.h>

#include "testmodbustcpconnection.h"

class Discovery : public QObject
{
    Q_OBJECT
public:
    explicit Discovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent = nullptr);
    struct Result {
        quint16 controllerType;
        NetworkDeviceInfo networkDeviceInfo;
    };

    void startDiscovery();

    QList<Result> discoveryResults() const;

signals:
    void discoveryFinished();
    void repliesFinished();

private:
    NetworkDeviceDiscovery *m_networkDeviceDiscovery = nullptr;

    QList<TestModbusTcpConnection *> m_connections;

    qint16 m_openReplies;

    QList<Result> m_discoveryResults;

    void checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo);
    void cleanupConnection(TestModbusTcpConnection *connection);
};

#endif // DISCOVERY_H
