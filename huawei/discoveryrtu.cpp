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

void DiscoveryRtu::startDiscovery()
{
    qCInfo(dcHuawei()) << "Discovery: Searching for Huawei inverter on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        if (master->dataBits() == 8 && master->stopBits() == 1 && master->parity() == QSerialPort::NoParity) {
            candidateMasters.append(master);
        }
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcHuawei()) << "No modbus RTU master with the required setting found (8 data bits, 1 stop bit, even parity).";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            tryConnect(master, 1);
        } else {
            qCWarning(dcHuawei()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<DiscoveryRtu::Result> DiscoveryRtu::discoveryResults() const
{
    return m_discoveryResults;
}

void DiscoveryRtu::tryConnect(ModbusRtuMaster *master, quint16 modbusId)
{
    qCDebug(dcHuawei()) << "Scanning modbus RTU master" << master->modbusUuid().toString() << "Modbus ID:" << modbusId;

    // Get the "meterGridFrequency" register. If this is a Huawei, a value close to 5000 must be in this register.
    // Nope, bad idea. Meter could be disconnected, then register for frequency will have value 0.
    ModbusRtuReply *reply = master->readHoldingRegister(modbusId, 30070);   // Temporary solution. This is model ID, just a value >0.
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcHuawei()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
            quint16 modelId = reply->result().first();

            if (modelId > 0) {
                qCDebug(dcHuawei()) << "Test register is Model ID, result is" << modelId;
                Result result;
                result.modbusId = modbusId;
                result.serialPort = QString(master->serialPort());
                result.modbusRtuMasterId = master->modbusUuid();
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcHuawei()) << "Value in test register 30070 should not be 0 but it is" << modelId << ". This is not a Huawei inverter.";
            }
        }
        if (modbusId < 20) {
            tryConnect(master, modbusId+1);
        } else {
            emit discoveryFinished(true);
        }
    });
}
