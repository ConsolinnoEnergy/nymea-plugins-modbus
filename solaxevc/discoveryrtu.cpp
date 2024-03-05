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

DiscoveryRtu::DiscoveryRtu(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource}
{
}

void ::startDiscovery()
{
    qCInfo(dcSolaxEvc()) << "Discovery: Searching for Solax wallboxes on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->baudrate() == 9600 && master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::NoParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcSolaxEvc()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            // Start searching with Modbus ID 70, the default Modbus ID of the wallbox.
            tryConnect(master, 70);

            // Knowledge so far, Solax wallboxes only have Modbus ID 70. If wrong, use following code to also search other Modbus IDs.
//            for (int modbusId = 1; modbusId < 11; ++modbusId) {
//                tryConnect(master, modbusId);
//            }
        } else {
            qCWarning(dcSolaxEvc()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
    emit discoveryFinished(true);
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcSolaxEvc()) << "Scanning modbus RTU master" << master->modbusUuid() << "Slave ID:" << modbusId;

    ModbusRtuReply *reply = master->readInputRegister(modbusId, 37);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcSolaxEvc()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() != ModbusRtuReply::NoError || reply->result().length() == 0) {
            qCDebug(dcSolaxEvc()) << "Error reading input register 37 (firmware version). This is not a Solax wallbox.";
        } else {
            quint16 version = reply->result().first();
            if (version > 0) {
                qCDebug(dcSolaxEvc()) << "Wallbox firmware version is " << version;

                ModbusRtuReply *reply2 = master->readInputRegister(modbusId, 41);
                connect(reply2, &ModbusRtuReply::finished, this, [=](){
                    qCDebug(dcSolaxEvc()) << "Reading next test value" << reply2->error() << reply2->result();
                    if (reply2->error() != ModbusRtuReply::NoError || reply2->result().length() == 0) {
                        qCDebug(dcSolaxEvc()) << "Error reading input register 41 (unbalanced power). This is not a Solax wallbox.";
                        return;
                    }

                    quint16 unbalancedPower = reply2->result().first();
                    if (unbalancedPower >= 1300 && unbalancedPower <= 7200) {
                        qCDebug(dcSolaxEvc()) << "Unbalanced power setting is" << unbalancedPower << ". Value is ok.";

                        ModbusRtuReply *reply3 = master->readInputRegister(modbusId, 33);
                        connect(reply3, &ModbusRtuReply::finished, this, [=](){
                            qCDebug(dcSolaxEvc()) << "Reading next test value" << reply3->error() << reply3->result();
                            if (reply3->error() != ModbusRtuReply::NoError || reply3->result().length() == 0) {
                                qCDebug(dcSolaxEvc()) << "Error reading input register 33 (type power). This is not a Solax wallbox.";
                                return;
                            }

                            quint16 typePower = reply3->result().first();
                            if (typePower <= 2) {
                                QString model{""};
                                switch (typePower) {
                                case 0:
                                    qCDebug(dcSolaxEvc()) << "This is a Solax X1-EVC-7.2K. Adding it to the list.";
                                    model = "Solax X1-EVC-7.2K";
                                    break;
                                case 1:
                                    qCDebug(dcSolaxEvc()) << "This is a Solax X3-EVC-11K. Adding it to the list.";
                                    model = "Solax X3-EVC-11K";
                                    break;
                                case 2:
                                    qCDebug(dcSolaxEvc()) << "This is a Solax X3-EVC-22K. Adding it to the list.";
                                    model = "Solax X3-EVC-22K";
                                    break;
                                default:
                                    qCWarning(dcSolaxEvc()) << "Error identifying Solax wallbox model. This should not happen.";
                                    model = "Error";
                                    break;
                                }

                                Result result {master->modbusUuid(), model, modbusId};
                                m_discoveryResults.append(result);
                            } else {
                                qCDebug(dcSolaxEvc()) << "Value in input register 33 (type power) should be in the range [0;2], but the value is"
                                                       << typePower << ". This is not a Solax wallbox.";
                            }
                        });

                    } else {
                        qCDebug(dcSolaxEvc()) << "Value in input register 41 (unbalanced power) should be in the range [1300;7200], but the value is"
                                               << unbalancedPower << ". This is not a Solax wallbox.";
                    }
                });

            } else {
                qCDebug(dcSolaxEvc()) << "Value in input register 37 (firmware version) should be >0, but the value is"
                                      << version << ". This is not a Solax wallbox.";
//                qCDebug(dcSolaxEvc()) << "Version must be at least xxx";
            }
        }
    });
}

