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

#include "rseriesdiscovery.h"
#include "extern-plugininfo.h"

DiscoveryRtu::DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource, uint modbusId, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource},
    m_modbusId{modbusId}
{
}

void DiscoveryRtu::startDiscovery()
{
    qCInfo(dcFoxess()) << "Discovery: Searching for smartmeter on modbus RTU...";

    if (m_modbusRtuResource->modbusRtuMasters().isEmpty()) {
        qCWarning(dcFoxess()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
    }

    m_openReplies = 0;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->connected()) {
            tryConnect(master, m_modbusId);
        } else {
            qCWarning(dcFoxess()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }

    if (m_openReplies < 0) {
        m_openReplies = 0;
    }
    if (m_openReplies) {
        qCDebug(dcFoxess()) << "Waiting for modbus RTU replies.";
        connect(this, &DiscoveryRtu::repliesFinished, this, [this](){
            emit discoveryFinished(true);
        });
    } else {
        emit discoveryFinished(true);
    }
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcFoxess()) << "Scanning modbus RTU master" << master->modbusUuid() << "Modbus ID:" << modbusId;

    m_openReplies++;
    ModbusRtuReply *reply = master->readHoldingRegister(modbusId, 40004, 16);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        m_openReplies--;
        qCDebug(dcFoxess()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() < 16) {
            qCDebug(dcFoxess()) << "Error reading holding register 40005.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }

        // Test frequency value.
        QString manufacturer = ModbusDataUtils::convertToString(reply->result());
        if (!manufacturer.contains("fox")) {
            qCWarning(dcFoxess()) << "Recieved value for manufacturer is" << manufacturer<< "This does not seem to be the correct value.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }
        qCDebug(dcFoxess()) << "Received value for manufacturer" << manufacturer << ". Value seems ok.";

        m_openReplies++;
        ModbusRtuReply *reply2 = master->readHoldingRegister(modbusId, 40052, 16);
        connect(reply2, &ModbusRtuReply::finished, this, [=](){
            m_openReplies--;
            qCDebug(dcFoxess()) << "Reading next test value" << reply2->error() << reply2->result();
            if (reply2->error() != ModbusRtuReply::NoError || reply2->result().length() < 16) {
                qCDebug(dcFoxess()) << "Error reading holding register 40053 (serial number).";
                if (m_openReplies <= 0) {
                    emit repliesFinished();
                }
                return;
            }

            QString serialNumber = ModbusDataUtils::convertToString(reply2->result());

            Result result {master->modbusUuid(), m_modbusId, master->serialPort(), serialNumber};

            qCWarning(dcFoxess()) << "Found a fox inverter with serialnumber" << serialNumber;
            m_discoveryResults.append(result);

            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
        });
    });
}

