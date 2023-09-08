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

#include "goodwediscovery.h"
#include "extern-plugininfo.h"

GoodweDiscovery::GoodweDiscovery(ModbusRtuHardwareResource *modbusRtuResource, QObject *parent) :
    QObject{parent},
    m_modbusRtuResource{modbusRtuResource}
{
}

void GoodweDiscovery::startDiscovery()
{
    qCInfo(dcGoodwe()) << "Discovery: Searching for Goodwe inverters on modbus RTU...";

    QList<ModbusRtuMaster*> candidateMasters;
    foreach (ModbusRtuMaster *master, m_modbusRtuResource->modbusRtuMasters()) {
        candidateMasters.append(master);
    }

    if (candidateMasters.isEmpty()) {
        qCWarning(dcGoodwe()) << "No usable modbus RTU master found.";
        emit discoveryFinished(false);
        return;
    }

    foreach (ModbusRtuMaster *master, candidateMasters) {
        if (master->connected()) {
            tryConnect(master, 247);
        } else {
            qCWarning(dcGoodwe()) << "Modbus RTU master" << master->modbusUuid().toString() << "is not connected.";
        }
    }
}

QList<GoodweDiscovery::Result> GoodweDiscovery::discoveryResults() const
{
    return m_discoveryResults;
}

void GoodweDiscovery::tryConnect(ModbusRtuMaster *master, quint16 slaveId)
{
    qCDebug(dcGoodwe()) << "Scanning modbus RTU master" << master->modbusUuid() << "Slave ID:" << slaveId;

    ModbusRtuReply *reply = master->readHoldingRegister(slaveId, 36014);
    connect(reply, &ModbusRtuReply::finished, this, [=](){
        qCDebug(dcGoodwe()) << "Test reply finished!" << reply->error() << reply->result();
        if (reply->error() == ModbusRtuReply::NoError && reply->result().length() > 0) {
            quint16 frequency = reply->result().first();
            if (frequency > 4900 && frequency < 5100) {
                qCDebug(dcGoodwe()) << "Detected grid frequency of " << frequency << " on holding register 36014.";
                Result result {master->modbusUuid(), slaveId};
                m_discoveryResults.append(result);
            } else {
                qCDebug(dcGoodwe()) << "Holding register 36014 should be grid frequency, but value is " << frequency << ". Seems this is not a Goodwe inverter.";
            }
        }

        // Discovery starts with slaveId 247, which is the default of the inverter. If it is not that, most probably slave Id is close to default (user changed it,
        // but not by much), and then single digit.
        if (slaveId > 244) {
            tryConnect(master, slaveId-1);
        } else {
            if (slaveId == 244) {
                tryConnect(master, 1);
            } else {
                if (slaveId < 10) {
                    tryConnect(master, slaveId+1);
                } else {
                    emit discoveryFinished(true);
                }
            }
        }
    });
}

