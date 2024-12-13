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
    qCInfo(dcJanitza()) << "Discovery: Searching for smartmeter on modbus RTU...";

    if (m_modbusRtuResource->modbusRtuMasters().isEmpty()) {
        qCWarning(dcJanitza()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
    }

    m_openReplies = 0;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->connected()) {
            tryConnect(master, m_modbusId);
        } else {
            qCWarning(dcJanitza()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }

    if (m_openReplies < 0) {
        m_openReplies = 0;
    }
    if (m_openReplies) {
        qCDebug(dcJanitza()) << "Waiting for modbus RTU replies.";
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
    qCDebug(dcJanitza()) << "Scanning modbus RTU master" << master->modbusUuid() << "Modbus ID:" << modbusId;
    qCDebug(dcJanitza()) << "Trying to read register 19050 (grid frequency) to test the connection.";

    m_openReplies++;
    ModbusRtuReply *reply = master->readInputRegister(modbusId, 19050, 2);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        m_openReplies--;
        qCDebug(dcJanitza()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() < 2) {
            qCDebug(dcJanitza()) << "Error reading input register 19050 (grid frequency). This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }

        // Test frequency value.
        float gridFrequency = ModbusDataUtils::convertToFloat32(reply->result(), m_endianness);
        if (gridFrequency < 49.0 || gridFrequency > 51.0) {
            qCDebug(dcJanitza()) << "Recieved value for grid frequency is" << gridFrequency << "Hz. This does not seem to be the correct value. This is not a smartmeter.";
            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
            return;
        }
        qCDebug(dcJanitza()) << "Recieved value for grid frequency is" << gridFrequency << "Hz. Value seems ok.";

        m_openReplies++;
        ModbusRtuReply *reply2 = master->readHoldingRegister(modbusId, 13437, 16);
        connect(reply2, &ModbusRtuReply::finished, this, [=](){
            m_openReplies--;
            qCDebug(dcJanitza()) << "Reading next test value" << reply2->error() << reply2->result();
            if (reply2->error() != ModbusRtuReply::NoError || reply2->result().length() < 16) {
                qCDebug(dcJanitza()) << "Error reading input register 13437 (firmware version). This is not a smartmeter.";
                if (m_openReplies <= 0) {
                    emit repliesFinished();
                }
                return;
            }

            QString firmwareVersion = ModbusDataUtils::convertToString(reply2->result().mid(0, 16));
            if (firmwareVersion != "5.029") {
                qCDebug(dcJanitza()) << "Received incorrect firmware version" << firmwareVersion;
                if (m_openReplies <= 0) {
                    emit repliesFinished();
                }
                return;
            }

            m_openReplies++;
            ModbusRtuReply *reply3 = master->readHoldingRegister(modbusId, 10176, 2);
            connect(reply3, &ModbusRtuReply::finished, this, [=](){
                m_openReplies--;
                if (reply3->error() != ModbusRtuReply::NoError || reply2->result().length() < 2) {
                    qCDebug(dcJanitza()) << "Error reading input register 10176 (serial number).";
                    if (m_openReplies <= 0) {
                        emit repliesFinished();
                    }
                    return;
                }
            });

            quint32 serialNumber = ModbusDataUtils::convertToUInt32(reply3->result(), m_endianness);

            Result result {master->modbusUuid(), firmwareVersion, master->serialPort(), serialNumber};

            m_discoveryResults.append(result);

            if (m_openReplies <= 0) {
                emit repliesFinished();
            }
        });
    });
}

