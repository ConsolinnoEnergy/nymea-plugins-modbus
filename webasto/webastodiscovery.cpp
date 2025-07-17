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

#include "webastodiscovery.h"
#include "extern-plugininfo.h"

#include <QTcpSocket>

WebastoDiscovery::WebastoDiscovery(NetworkDeviceDiscovery *networkDeviceDiscovery, QObject *parent)
    : QObject{parent},
      m_networkDeviceDiscovery{networkDeviceDiscovery}
{

}

void WebastoDiscovery::startDiscovery()
{
    m_startDateTime = QDateTime::currentDateTime();

    qCInfo(dcWebasto()) << "Discovery: Starting to search for WebastoNext wallboxes in the network...";
    NetworkDeviceDiscoveryReply *discoveryReply = m_networkDeviceDiscovery->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::networkDeviceInfoAdded, this, &WebastoDiscovery::checkNetworkDevice);
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        qCDebug(dcWebasto()) << "Discovery: Network discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "network devices";
        // Give the last connections added right before the network discovery finished a chance to check the device...
        QTimer::singleShot(3000, this, [this](){
            qCDebug(dcWebasto()) << "Discovery: Grace period timer triggered.";
            finishDiscovery();
        });
        discoveryReply->deleteLater();
    });
}

QList<WebastoDiscovery::Result> WebastoDiscovery::results() const
{
    return m_results;
}

void WebastoDiscovery::checkNetworkDevice(const NetworkDeviceInfo &networkDeviceInfo)
{
    WebastoNextModbusTcpConnection *connection = new WebastoNextModbusTcpConnection(networkDeviceInfo.address(), 502, 1, this);
    m_connections.append(connection);

    connect(connection, &WebastoNextModbusTcpConnection::reachableChanged, this, [=](bool reachable){
        if (!reachable) {
            // Disconnected ... done with this connection
            cleanupConnection(connection);
            return;
        }

        // Read some well known registers to verify if the register exist and make sense...
        QModbusReply *reply = connection->readMaxChargingCurrent();
        connect(reply, &QModbusReply::finished, this, [=](){

            reply->deleteLater();

            if (reply->error() != QModbusDevice::NoError) {
                // Something went wrong...probably not the device we are searching for
                cleanupConnection(connection);
                return;
            }

            // Check if the value makes sense.
            const QModbusDataUnit unit = reply->result();
            quint16 maxChargingCurrent = ModbusDataUtils::convertToUInt16(unit.values());
            qCDebug(dcWebasto()) << "Discovery: reading register maxChargingCurrent from" << networkDeviceInfo.address().toString() << ", result is" << maxChargingCurrent;
            if (maxChargingCurrent < 1 || maxChargingCurrent > 32) {
                qCDebug(dcWebasto()) << "Discovery: expected a value between 1 and 32 in register maxChargingCurrent, but the value is" << maxChargingCurrent << ". This is not a Webasto Next wallbox.";
                cleanupConnection(connection);
                return;
            }

            QModbusReply *reply = connection->readMinChargingCurrent();
            connect(reply, &QModbusReply::finished, this, [=](){

                reply->deleteLater();

                if (reply->error() != QModbusDevice::NoError) {
                    // Something went wrong...probably not the device we are searching for
                    cleanupConnection(connection);
                    return;
                }

                // Check if the value makes sense.
                const QModbusDataUnit unit = reply->result();
                quint16 minChargingCurrent = ModbusDataUtils::convertToUInt16(unit.values());
                qCDebug(dcWebasto()) << "Discovery: reading register minChargingCurrent from" << networkDeviceInfo.address().toString() << ", result is" << minChargingCurrent;
                if (minChargingCurrent < 1 || minChargingCurrent > 32) {
                    qCDebug(dcWebasto()) << "Discovery: expected a value between 1 and 32 in register minChargingCurrent, but the value is" << minChargingCurrent << ". This is not a Webasto Next wallbox.";
                    cleanupConnection(connection);
                    return;
                }

                QModbusReply *reply = connection->readComTimeout();
                connect(reply, &QModbusReply::finished, this, [=](){

                    reply->deleteLater();

                    if (reply->error() != QModbusDevice::NoError) {
                        // Something went wrong...probably not the device we are searching for
                        cleanupConnection(connection);
                        return;
                    }

                    // Check if the value makes sense.
                    const QModbusDataUnit unit = reply->result();
                    quint16 comTimeout = ModbusDataUtils::convertToUInt16(unit.values());
                    qCDebug(dcWebasto()) << "Discovery: reading register comTimeout from" << networkDeviceInfo.address().toString() << ", result is" << comTimeout;
                    if (minChargingCurrent < 1) {
                        qCDebug(dcWebasto()) << "Discovery: expected a value >0 in register comTimeout, but the value is" << comTimeout << ". This is not a Webasto Next wallbox.";
                        cleanupConnection(connection);
                        return;
                    }

                    // All values good so far, let's assume this is a Webasto NEXT

                    Result result;
                    result.productName = "Webasto NEXT";
                    result.type = TypeWebastoNext;
                    result.networkDeviceInfo = networkDeviceInfo;
                    m_results.append(result);

                    qCDebug(dcWebasto()) << "Discovery: --> Found" << result.productName << result.networkDeviceInfo;

                    // Done with this connection
                    cleanupConnection(connection);
                });
            });
        });
    });

    // If we get any error...skip this host...
    connect(connection, &ModbusTcpMaster::connectionErrorOccurred, this, [=](QModbusDevice::Error error){
        if (error != QModbusDevice::NoError) {
            qCDebug(dcWebasto()) << "Discovery: Connection error on" << networkDeviceInfo.address().toString() << "Continue...";;
            cleanupConnection(connection);
        }
    });

    // If check reachability failed...skip this host...
    connect(connection, &WebastoNextModbusTcpConnection::checkReachabilityFailed, this, [=](){
        qCDebug(dcWebasto()) << "Discovery: Check reachability failed on" << networkDeviceInfo.address().toString() << "Continue...";;
        cleanupConnection(connection);
    });

    // Try to connect, maybe it works, maybe not...
    connection->connectDevice();
}

void WebastoDiscovery::cleanupConnection(WebastoNextModbusTcpConnection *connection)
{
    m_connections.removeAll(connection);
    connection->disconnectDevice();
    connection->deleteLater();
}

void WebastoDiscovery::finishDiscovery()
{
    qint64 durationMilliSeconds = QDateTime::currentMSecsSinceEpoch() - m_startDateTime.toMSecsSinceEpoch();

    // Cleanup any leftovers...we don't care any more
    foreach (WebastoNextModbusTcpConnection *connection, m_connections)
        cleanupConnection(connection);

    qCInfo(dcWebasto()) << "Discovery: Finished the discovery process. Found" << m_results.count() << "Webasto NEXT wallboxes in" << QTime::fromMSecsSinceStartOfDay(durationMilliSeconds).toString("mm:ss.zzz");
    emit discoveryFinished();
}
