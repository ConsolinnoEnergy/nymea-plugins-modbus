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

#include "discoveryrtu.h"
#include "extern-plugininfo.h"

DiscoveryRtu::DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource, uint modbusId, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource},
    m_modbusId{modbusId},
    m_endianness{ModbusDataUtils::ByteOrderBigEndian}
{
}

void DiscoveryRtu::startDiscovery()
{
    qCInfo(dcBgeTech()) << "Discovery: Searching for smartmeter on modbus RTU...";

    if (m_modbusRtuResource->modbusRtuMasters().isEmpty()) {
        qCWarning(dcBgeTech()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
    }

    m_openReplies = 0;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->connected()) {
            tryConnect(master, m_modbusId);
        } else {
            qCWarning(dcBgeTech()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }

    if (m_openReplies < 0) {
        m_openReplies = 0;
    }
    if (m_openReplies) {
        qCDebug(dcBgeTech()) << "Waiting for modbus RTU replies.";
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
    qCDebug(dcBgeTech()) << "Scanning modbus RTU master" << master->modbusUuid() << "Modbus ID:" << modbusId;
    qCDebug(dcBgeTech()) << "Trying to read register 70 (grid frequency) to test the connection.";

    m_openReplies++;
    ModbusRtuReply *reply = master->readInputRegister(modbusId, 70, 2);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        m_openReplies--;
        qCDebug(dcBgeTech()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() < 2) {
            qCDebug(dcBgeTech()) << "Error reading input register 70 (grid frequency). This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }

        // Test frequency value.
        float gridFrequency = ModbusDataUtils::convertToFloat32(reply->result(), m_endianness);
        if (gridFrequency < 49.0 || gridFrequency > 51.0) {
            qCDebug(dcBgeTech()) << "Recieved value for grid frequency is" << gridFrequency << "Hz. This does not seem to be the correct value. This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }
        qCDebug(dcBgeTech()) << "Recieved value for grid frequency is" << gridFrequency << "Hz. Value seems ok.";

        m_openReplies++;
        ModbusRtuReply *reply2 = master->readHoldingRegister(modbusId, 64512, 3);
        connect(reply2, &ModbusRtuReply::finished, this, [=](){
            m_openReplies--;
            qCDebug(dcBgeTech()) << "Reading next test value" << reply2->error() << reply2->result();
            if (reply2->error() != ModbusRtuReply::NoError || reply2->result().length() < 3) {
                qCDebug(dcBgeTech()) << "Error reading input register 64512 (serial number). This is not a smartmeter.";
                if (m_openReplies <= 0) {
                    emit repliesFinished();
                }
                return;
            }

            quint32 serialNumber = ModbusDataUtils::convertToUInt32(reply2->result().mid(0, 2), m_endianness);
            quint16 meterCode = ModbusDataUtils::convertToUInt32(reply2->result().mid(2, 1));

            // Add test to check if values are valid. meterCode supposedly can identify what type of device it is (SDM630 or SDM72).

            Result result {master->modbusUuid(), serialNumber, meterCode, master->serialPort()};

            qCDebug(dcBgeTech()) << "Found a smartmeter with serial number" << serialNumber << "and meter code" << meterCode << ". Adding it to the list.";
            m_discoveryResults.append(result);

            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
        });
    });
}

