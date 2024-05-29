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

#include "energycontroldiscovery.h"
#include "extern-plugininfo.h"

#include <modbusdatautils.h>

EnergyControlDiscovery::EnergyControlDiscovery(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource}
{
}

void EnergyControlDiscovery::startDiscovery()
{
    qCInfo(dcAmperfied()) << "Discovery: Searching for Amperfied EnergyControl wallboxes on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->baudrate() == 19200 && master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::EvenParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcAmperfied()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            // Start searching with Modbus ID 1.
            tryConnect(master, 1);
        } else {
            qCWarning(dcAmperfied()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<EnergyControlDiscovery::Result> EnergyControlDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void EnergyControlDiscovery::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcAmperfied()) << "Scanning modbus RTU master" << master->modbusUuid() << "Slave ID:" << modbusId;

    ModbusRtuReply *reply = master->readInputRegister(modbusId, 4);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcAmperfied()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() == 0) {
            qCDebug(dcAmperfied()) << "Error reading input register 4 (firmware version). This is not an Energy Control wallbox.";
        } else {
            quint16 version = reply->result().first();
            if (version >= 0x0107) {
                qCDebug(dcAmperfied()) << QString("Version is 0x%1").arg(version, 0, 16);

                ModbusRtuReply *reply2 = master->readInputRegister(modbusId, 5);
                connect(reply2, &ModbusRtuReply::finished, this, [=](){
                    qCDebug(dcAmperfied()) << "Reading next test value" << reply2->error() << reply2->result();
                    if (reply2->error() != ModbusRtuReply::NoError || reply2->result().length() == 0) {
                        qCDebug(dcAmperfied()) << "Error reading input register 5 (charging state). This is not an Energy Control wallbox.";
                        return;
                    }

                    quint16 chargingState = reply2->result().first();
                    if (chargingState >= 1 && chargingState <= 11) {
                        qCDebug(dcAmperfied()) << "Charging state is" << chargingState << ". Value is ok.";


                        ModbusRtuReply *reply3 = master->readInputRegister(modbusId, 100, 34);
                        connect(reply3, &ModbusRtuReply::finished, this, [=](){
                            qCDebug(dcAmperfied()) << "Reading next test value" << reply3->error() << reply3->result();
                            if (reply3->error() != ModbusRtuReply::NoError || reply3->result().length() == 0) {
                                qCDebug(dcAmperfied()) << "Error reading input registers 100 to 133. This is not an Energy Control wallbox.";
                                return;
                            }

                            quint16 maximumCurrent = ModbusDataUtils::convertToUInt16(reply3->result().mid(0, 1));
                            quint16 minimumCurrent = ModbusDataUtils::convertToUInt16(reply3->result().mid(1, 1));
                            QString serialNumber = ModbusDataUtils::convertToString(reply3->result().mid(2, 31));

                            if (maximumCurrent >= 6 && maximumCurrent <= 16) {
                                qCDebug(dcAmperfied()) << "Maximum charge current is" << maximumCurrent << ". Value is ok.";

                                if (minimumCurrent >= 6 && minimumCurrent <= 16) {
                                    qCDebug(dcAmperfied()) << "Minimum charge current is" << minimumCurrent << ". Value is ok.";

                                    if (serialNumber.length() > 0) {
                                        Result result {master->modbusUuid(), version, modbusId, master->serialPort(), serialNumber};
                                        qCDebug(dcAmperfied()) << "This device is an Energy Control wallbox. Adding it to the list.";
                                        m_discoveryResults.append(result);
                                    } else {
                                        qCDebug(dcAmperfied()) << "Value in input register 102 (serial number) should be a string, but the value is"
                                                               << serialNumber << ". This is not an Energy Control wallbox.";
                                    }
                                } else {
                                    qCDebug(dcAmperfied()) << "Value in input register 101 (minimum charge current) should be in the range [6;16], but the value is"
                                                           << maximumCurrent << ". This is not an Energy Control wallbox.";
                                }
                            } else {
                                qCDebug(dcAmperfied()) << "Value in input register 100 (maximum charge current) should be in the range [6;16], but the value is"
                                                       << maximumCurrent << ". This is not an Energy Control wallbox.";
                            }


                        });


                    } else {
                        qCDebug(dcAmperfied()) << "Value in input register 5 (charging state) should be in the range [1;11], but the value is"
                                               << chargingState << ". This is not an Energy Control wallbox.";
                    }
                });

            } else {
                qCDebug(dcAmperfied()) << QString("Version is 0x%1, but version needs to be at least 1.0.7 (0x0107)").arg(version, 0, 16);
            }
        }

        // The possible Modbus IDs of the Energy Control are in the range [1;15].
        if (modbusId < 15) {
            tryConnect(master, modbusId+1);
        } else {
            emit discoveryFinished(true);
        }
    });
}

